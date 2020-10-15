/*
 * src/bin/pg_autoctl/pghba.c
 *	 Functions for manipulating pg_hba.conf
 *
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the PostgreSQL License.
 *
 */
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "postgres_fe.h"
#include "pqexpbuffer.h"

#include "defaults.h"
#include "file_utils.h"
#include "ipaddr.h"
#include "parsing.h"
#include "pgctl.h"
#include "pghba.h"
#include "log.h"


#define HBA_LINE_COMMENT " # Auto-generated by pg_auto_failover"

static bool pghba_append_rule_to_buffer(PQExpBuffer buffer,
										bool ssl,
										HBADatabaseType databaseType,
										const char *database,
										const char *username,
										const char *host,
										const char *authenticationScheme);
static void append_database_field(PQExpBuffer destination,
								  HBADatabaseType databaseType,
								  const char *databaseName);
static void append_hostname_or_cidr(PQExpBuffer destination,
									const char *host);
static int escape_hba_string(char *destination, const char *hbaString);


/*
 * pghba_append_rule_to_buffer creates a new HBA rule with the given database,
 * username, host and authentication scheme in the given buffer.
 */
static bool
pghba_append_rule_to_buffer(PQExpBuffer buffer,
							bool ssl,
							HBADatabaseType databaseType,
							const char *database,
							const char *username,
							const char *host,
							const char *authenticationScheme)
{
	if (ssl)
	{
		appendPQExpBufferStr(buffer, "hostssl ");
	}
	else
	{
		appendPQExpBufferStr(buffer, "host ");
	}

	append_database_field(buffer, databaseType, database);
	appendPQExpBufferStr(buffer, " ");

	if (username)
	{
		char escapedUsername[BUFSIZE] = { 0 };
		(void) escape_hba_string(escapedUsername, username);

		appendPQExpBufferStr(buffer, escapedUsername);
		appendPQExpBufferStr(buffer, " ");
	}
	else
	{
		appendPQExpBufferStr(buffer, "all ");
	}

	append_hostname_or_cidr(buffer, host);
	appendPQExpBuffer(buffer, " %s", authenticationScheme);

	/* memory allocation could have failed while building string */
	if (PQExpBufferBroken(buffer))
	{
		log_error("Failed to allocate memory");
		destroyPQExpBuffer(buffer);
		return false;
	}

	return true;
}


/*
 * pghba_ensure_host_rule_exists ensures that a host rule exists in the
 * pg_hba file with the given database, username, host and authentication
 * scheme.
 */
bool
pghba_ensure_host_rule_exists(const char *hbaFilePath,
							  bool ssl,
							  HBADatabaseType databaseType,
							  const char *database,
							  const char *username,
							  const char *host,
							  const char *authenticationScheme)
{
	char *currentHbaContents = NULL;
	long currentHbaSize = 0L;
	char *includeLine = NULL;
	PQExpBuffer hbaLineBuffer = createPQExpBuffer();
	PQExpBuffer newHbaContents = NULL;

	if (hbaLineBuffer == NULL)
	{
		log_error("Failed to allocate memory");
		return false;
	}

	if (!pghba_append_rule_to_buffer(hbaLineBuffer,
									 ssl,
									 databaseType,
									 database,
									 username,
									 host,
									 authenticationScheme))
	{
		/* errors have already been logged */

		/* done with the new HBA line buffer */
		destroyPQExpBuffer(hbaLineBuffer);

		return false;
	}

	log_debug("Ensuring the HBA file \"%s\" contains the line: %s",
			  hbaFilePath, hbaLineBuffer->data);

	if (!read_file(hbaFilePath, &currentHbaContents, &currentHbaSize))
	{
		/* read_file logs an error */

		/* done with the new HBA line buffer */
		destroyPQExpBuffer(hbaLineBuffer);

		return false;
	}

	includeLine = strstr(currentHbaContents, hbaLineBuffer->data);

	/*
	 * If the rule was found and it starts on a new line. We can
	 * skip adding it.
	 */
	if (includeLine != NULL && (includeLine == currentHbaContents ||
								includeLine[-1] == '\n'))
	{
		log_debug("Line already exists in %s, skipping %s",
				  hbaFilePath, hbaLineBuffer->data);

		destroyPQExpBuffer(hbaLineBuffer);
		free(currentHbaContents);
		return true;
	}

	/*
	 * When the authentication method is "skip", the option --skip-pg-hba has
	 * been used. In that case, we still WARN about the HBA rule that we need,
	 * so that users can review their HBA settings and provisioning.
	 */
	if (SKIP_HBA(authenticationScheme))
	{
		log_warn("Skipping HBA edits (per --skip-pg-hba) for rule: %s",
				 hbaLineBuffer->data);
		destroyPQExpBuffer(hbaLineBuffer);
		free(currentHbaContents);
		return true;
	}

	/*
	 * When using a hostname in the HBA host field, Postgres is very picky
	 * about the matching rules. We have an opportunity here to check the same
	 * DNS and reverse DNS rules as Postgres, and warn our users when we see
	 * something that we know Postgres won't be happy with.
	 *
	 * HBA & DNS is hard.
	 */
	(void) pghba_check_hostname(host);

	/* build the new postgresql.conf contents */
	newHbaContents = createPQExpBuffer();
	if (newHbaContents == NULL)
	{
		log_error("Failed to allocate memory");
		destroyPQExpBuffer(hbaLineBuffer);
		free(currentHbaContents);
		return false;
	}

	appendPQExpBufferStr(newHbaContents, currentHbaContents);
	appendPQExpBufferStr(newHbaContents, hbaLineBuffer->data);
	appendPQExpBufferStr(newHbaContents, HBA_LINE_COMMENT "\n");

	/* done with the old pg_hba.conf contents */
	free(currentHbaContents);

	/* done with the new HBA line buffer */
	destroyPQExpBuffer(hbaLineBuffer);

	/* memory allocation could have failed while building string */
	if (PQExpBufferBroken(newHbaContents))
	{
		log_error("Failed to allocate memory");
		destroyPQExpBuffer(newHbaContents);
		return false;
	}

	/* write the new pg_hba.conf */
	if (!write_file(newHbaContents->data, newHbaContents->len, hbaFilePath))
	{
		/* write_file logs an error */
		destroyPQExpBuffer(newHbaContents);
		return false;
	}

	destroyPQExpBuffer(newHbaContents);

	log_debug("Wrote new %s", hbaFilePath);

	return true;
}


/*
 * pghba_ensure_host_rules_exist ensures that we have all the rules needed for
 * the given array of nodes, as retrived from the monitor for our formation and
 * group, presumably.
 *
 * Each node in the array needs two rules:
 *
 *  host(ssl) replication "pgautofailover_replicator" hostname/ip trust
 *  host(ssl) "dbname"    "pgautofailover_replicator" hostname/ip trust
 */
bool
pghba_ensure_host_rules_exist(const char *hbaFilePath,
							  NodeAddressArray *nodesArray,
							  bool ssl,
							  const char *database,
							  const char *username,
							  const char *authenticationScheme)
{
	PQExpBuffer newHbaContents = createPQExpBuffer();
	char *currentHbaContents = NULL;
	long currentHbaSize = 0L;
	char *includeLine = NULL;

	int nodeIndex = 0;

	PQExpBuffer hbaLineReplicationBuffer = NULL;
	PQExpBuffer hbaLineDatabaseBuffer = NULL;

	if (newHbaContents == NULL)
	{
		log_error("Failed to allocate memory");
		return false;
	}

	if (!read_file(hbaFilePath, &currentHbaContents, &currentHbaSize))
	{
		/* read_file logs an error */
		return false;
	}

	/* always begin with the existing HBA file */
	appendPQExpBufferStr(newHbaContents, currentHbaContents);

	for (nodeIndex = 0; nodeIndex < nodesArray->count; nodeIndex++)
	{
		NodeAddress *node = &(nodesArray->nodes[nodeIndex]);

		int hbaLinesIndex = 0;
		PQExpBuffer hbaLines[3] = { 0 };

		/* done with the new HBA line buffers (and safe to call on NULL) */
		destroyPQExpBuffer(hbaLineReplicationBuffer);
		destroyPQExpBuffer(hbaLineDatabaseBuffer);

		/* we need new buffers now */
		hbaLineReplicationBuffer = createPQExpBuffer();
		hbaLineDatabaseBuffer = createPQExpBuffer();

		if (hbaLineReplicationBuffer == NULL ||
			hbaLineDatabaseBuffer == NULL)
		{
			log_error("Failed to allocate memory");

			/* done with the old pg_hba.conf contents */
			free(currentHbaContents);

			/* done with the new HBA line buffers */
			destroyPQExpBuffer(hbaLineReplicationBuffer);
			destroyPQExpBuffer(hbaLineDatabaseBuffer);

			return false;
		}

		log_debug("pghba_ensure_host_rules_exist: %d %s:%d",
				  node->nodeId, node->host, node->port);

		if (!SKIP_HBA(authenticationScheme))
		{
			/*
			 * When using a hostname in the HBA host field, Postgres is very
			 * picky about the matching rules. We have an opportunity here to
			 * check the same DNS and reverse DNS rules as Postgres, and warn
			 * our users when we see something that we know Postgres won't be
			 * happy with.
			 *
			 * HBA & DNS is hard.
			 */
			(void) pghba_check_hostname(node->host);
		}

		if (!pghba_append_rule_to_buffer(hbaLineReplicationBuffer,
										 ssl,
										 HBA_DATABASE_REPLICATION,
										 NULL,
										 username,
										 node->host,
										 authenticationScheme))
		{
			/* errors have already been logged */

			/* done with the old pg_hba.conf contents */
			free(currentHbaContents);

			/* done with the new HBA line buffers (and safe to call on NULL) */
			destroyPQExpBuffer(hbaLineReplicationBuffer);
			destroyPQExpBuffer(hbaLineDatabaseBuffer);

			return false;
		}

		if (!pghba_append_rule_to_buffer(hbaLineDatabaseBuffer,
										 ssl,
										 HBA_DATABASE_DBNAME,
										 database,
										 username,
										 node->host,
										 authenticationScheme))
		{
			/* errors have already been logged */

			/* done with the old pg_hba.conf contents */
			free(currentHbaContents);

			/* done with the new HBA line buffers (and safe to call on NULL) */
			destroyPQExpBuffer(hbaLineReplicationBuffer);
			destroyPQExpBuffer(hbaLineDatabaseBuffer);

			return false;
		}

		log_info("Ensuring HBA rules for node %d (%s:%d)",
				 node->nodeId, node->host, node->port);

		hbaLines[0] = hbaLineReplicationBuffer;
		hbaLines[1] = hbaLineDatabaseBuffer;

		for (hbaLinesIndex = 0; hbaLines[hbaLinesIndex] != NULL; hbaLinesIndex++)
		{
			PQExpBuffer hbaLineBuffer = hbaLines[hbaLinesIndex];

			log_debug("Ensuring the HBA file \"%s\" contains the line: %s",
					  hbaFilePath, hbaLineBuffer->data);

			includeLine = strstr(currentHbaContents, hbaLineBuffer->data);

			/*
			 * If the rule was found and it starts on a new line. We can
			 * skip adding it.
			 */
			if (includeLine != NULL && (includeLine == currentHbaContents ||
										includeLine[-1] == '\n'))
			{
				log_debug("Line already exists in %s, skipping %s",
						  hbaFilePath, hbaLineBuffer->data);

				continue;
			}

			/*
			 * When the authentication method is "skip", the option
			 * --skip-pg-hba has been used. In that case, we still WARN about
			 * the HBA rule that we need, so that users can review their HBA
			 * settings and provisioning.
			 */
			if (SKIP_HBA(authenticationScheme))
			{
				log_warn("Skipping HBA edits (per --skip-pg-hba) for rule: %s",
						 hbaLineBuffer->data);

				continue;
			}

			/* now append the line to the new HBA file contents */
			appendPQExpBufferStr(newHbaContents, hbaLineBuffer->data);
			appendPQExpBufferStr(newHbaContents, HBA_LINE_COMMENT "\n");
		}
	}

	/* done with the new HBA line buffers (and safe to call on NULL) */
	destroyPQExpBuffer(hbaLineReplicationBuffer);
	destroyPQExpBuffer(hbaLineDatabaseBuffer);

	/* done with the old pg_hba.conf contents */
	free(currentHbaContents);

	/* memory allocation could have failed while building string */
	if (PQExpBufferBroken(newHbaContents))
	{
		log_error("Failed to allocate memory");
		destroyPQExpBuffer(newHbaContents);
		return false;
	}

	/* write the new pg_hba.conf */
	if (!write_file(newHbaContents->data, newHbaContents->len, hbaFilePath))
	{
		/* write_file logs an error */
		destroyPQExpBuffer(newHbaContents);
		return false;
	}

	destroyPQExpBuffer(newHbaContents);

	log_debug("Wrote new %s", hbaFilePath);

	return true;
}


/*
 * append_database_field writes the database field to destination according to
 * the databaseType. If the type is HBA_DATABASE_DBNAME then the databaseName
 * is written in quoted form.
 */
static void
append_database_field(PQExpBuffer destination,
					  HBADatabaseType databaseType,
					  const char *databaseName)
{
	switch (databaseType)
	{
		case HBA_DATABASE_ALL:
		{
			appendPQExpBufferStr(destination, "all");
			break;
		}

		case HBA_DATABASE_REPLICATION:
		{
			appendPQExpBufferStr(destination, "replication");
			break;
		}

		case HBA_DATABASE_DBNAME:
		default:
		{
			/* Postgres database names are NAMEDATALEN (64), BUFSIZE is 1024 */
			char escapedDatabaseName[BUFSIZE] = { 0 };
			(void) escape_hba_string(escapedDatabaseName, databaseName);

			appendPQExpBufferStr(destination, escapedDatabaseName);
			break;
		}
	}
}


/*
 * append_hostname_or_cidr checks whether the host is an IP and if so converts
 * it to a CIDR and writes it to destination. Otherwise, convert_ip_to_cidr
 * writes the host directly to the destination.
 */
static void
append_hostname_or_cidr(PQExpBuffer destination, const char *host)
{
	switch (ip_address_type(host))
	{
		case IPTYPE_V4:
		{
			appendPQExpBuffer(destination, "%s/32", host);
			break;
		}

		case IPTYPE_V6:
		{
			appendPQExpBuffer(destination, "%s/128", host);
			break;
		}

		case IPTYPE_NONE:
		default:
		{
			appendPQExpBufferStr(destination, host);
			break;
		}
	}
}


/*
 * escape_hba_string escapes a string that is used in a pg_hba.conf file
 * and writes it to the destination. escape_hba_string returns the number
 * of characters written.
 *
 * While this is not documented, the code in hba.c (next_token) implements
 * two double-quotes as a literal double quote.
 */
static int
escape_hba_string(char *destination, const char *hbaString)
{
	int charIndex = 0;
	int length = strlen(hbaString);
	int escapedStringLength = 0;

	destination[escapedStringLength++] = '"';

	for (charIndex = 0; charIndex < length; charIndex++)
	{
		char currentChar = hbaString[charIndex];
		if (currentChar == '"')
		{
			destination[escapedStringLength++] = '"';
		}

		destination[escapedStringLength++] = currentChar;
	}

	destination[escapedStringLength++] = '"';
	destination[escapedStringLength] = '\0';

	return escapedStringLength;
}


/*
 * pghba_enable_lan_cidr adds our local CIDR network notation (e.g.
 * 192.168.0.0/23) to the HBA file of the PostgreSQL server, so that any node
 * in the local network may connect already.
 *
 * Failure is a warning only.
 *
 * In normal cases, pgdata is NULL and pghba_enable_lan_cidr queries the local
 * PostgreSQL server for the location of its HBA file.
 *
 * When initializing a PostgreSQL cluster in a test environment using
 * PG_REGRESS_SOCK_DIR="" and --listen options, then we have to add an HBA rule
 * before starting PostgreSQL, otherwise we don't have a path to connect to it.
 * In that case we pass in PGDATA and pghba_enable_lan_cidr uses the file
 * PGDATA/pg_hba.conf as the hbaFilePath: we just did `pg_ctl initdb` after
 * all, it should be safe.
 */
bool
pghba_enable_lan_cidr(PGSQL *pgsql,
					  bool ssl,
					  HBADatabaseType databaseType,
					  const char *database,
					  const char *hostname,
					  const char *username,
					  const char *authenticationScheme,
					  const char *pgdata)
{
	char hbaFilePath[MAXPGPATH];
	char ipAddr[BUFSIZE];
	char cidr[BUFSIZE];

	/* Compute the CIDR notation for our hostname */
	if (!findHostnameLocalAddress(hostname, ipAddr, BUFSIZE))
	{
		int logLevel = SKIP_HBA(authenticationScheme) ? LOG_WARN : LOG_FATAL;

		log_level(logLevel,
				  "Failed to find IP address for hostname \"%s\", "
				  "see above for details",
				  hostname);

		/* when --skip-pg-hba is used, we don't mind the failure here */
		return SKIP_HBA(authenticationScheme) ? true : false;
	}

	if (!fetchLocalCIDR(ipAddr, cidr, BUFSIZE))
	{
		log_warn("Failed to determine network configuration for "
				 "IP address \"%s\", skipping HBA settings", ipAddr);

		/* when --skip-pg-hba is used, we don't mind the failure here */
		return SKIP_HBA(authenticationScheme) ? true : false;
	}

	log_debug("HBA: adding CIDR from hostname \"%s\"", hostname);
	log_debug("HBA: local ip address: %s", ipAddr);
	log_debug("HBA: CIDR address to open: %s", cidr);

	log_info("Granting connection privileges on %s", cidr);

	/* The caller gives pgdata when PostgreSQL is not yet running */
	if (pgdata == NULL)
	{
		if (!pgsql_get_hba_file_path(pgsql, hbaFilePath, MAXPGPATH))
		{
			/* unexpected */
			log_error("Failed to obtain the HBA file path from the local "
					  "PostgreSQL server.");
			return false;
		}
	}
	else
	{
		sformat(hbaFilePath, MAXPGPATH, "%s/pg_hba.conf", pgdata);
	}

	/*
	 * We still go on when skipping HBA, so that we display a useful message to
	 * the user with the specific rule we are skipping here.
	 */
	if (!pghba_ensure_host_rule_exists(hbaFilePath, ssl, databaseType, database,
									   username, cidr, authenticationScheme))
	{
		log_error("Failed to add the local network to PostgreSQL HBA file: "
				  "couldn't modify the pg_hba file");
		return false;
	}

	/*
	 * pgdata is given when PostgreSQL is not yet running, don't reload then...
	 */
	if (pgdata == NULL &&
		!SKIP_HBA(authenticationScheme) &&
		!pgsql_reload_conf(pgsql))
	{
		log_error("Failed to reload PostgreSQL configuration for new HBA rule");
		return false;
	}
	return true;
}


/*
 * hba_check_hostname returns true when the DNS setting looks compatible with
 * Postgres expectations for an HBA hostname entry.
 *
 * https://www.postgresql.org/docs/current/auth-pg-hba-conf.html
 *
 * If a host name is specified (anything that is not an IP address range or a
 * special key word is treated as a host name), that name is compared with the
 * result of a reverse name resolution of the client's IP address (e.g.,
 * reverse DNS lookup, if DNS is used). Host name comparisons are case
 * insensitive. If there is a match, then a forward name resolution (e.g.,
 * forward DNS lookup) is performed on the host name to check whether any of
 * the addresses it resolves to are equal to the client's IP address. If both
 * directions match, then the entry is considered to match. (The host name that
 * is used in pg_hba.conf should be the one that address-to-name resolution of
 * the client's IP address returns, otherwise the line won't be matched. Some
 * host name databases allow associating an IP address with multiple host
 * names, but the operating system will only return one host name when asked to
 * resolve an IP address.)
 */
bool
pghba_check_hostname(const char *hostname)
{
	char ipaddr[BUFSIZE] = { 0 };

	/*
	 * IP addresses do not require any DNS properties/lookups. Also hostname
	 * won't contain a '/' character, but CIDR notations would, such as
	 * 1.2.3.4/32 or ::1/128. We don't want to trust ip_address_type() value of
	 * IPTYPE_NONE when we find a '/' character in the hostname.
	 *
	 */
	if (strchr(hostname, '/') || ip_address_type(hostname) != IPTYPE_NONE)
	{
		return true;
	}

	if (!resolveHostnameForwardAndReverse(hostname, ipaddr, sizeof(ipaddr)))
	{
		/* warn users about possible DNS misconfiguration */
		log_warn("Failed to resolve hostname \"%s\" to an IP address that "
				 "resolves batck to the hostname on a reverse DNS lookup.",
				 hostname);

		log_warn("Postgres might deny connection attempts from \"%s\", "
				 "even with the new HBA rules.",
				 hostname);

		return false;
	}

	log_debug("pghba_check_hostname: \"%s\" <-> %s", hostname, ipaddr);

	return true;
}
