/*
 * src/bin/pg_autoctl/monitor_config.c
 *     Monitor configuration functions
 *
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the PostgreSQL License.
 *
 */

#include <stdbool.h>
#include <unistd.h>

#include "postgres_fe.h"

#include "config.h"
#include "defaults.h"
#include "ini_file.h"
#include "ipaddr.h"
#include "monitor.h"
#include "monitor_config.h"
#include "log.h"
#include "pgctl.h"

#define OPTION_AUTOCTL_ROLE(config) \
	make_strbuf_option_default("pg_autoctl", "role", NULL, true, NAMEDATALEN, \
							   config->role, MONITOR_ROLE)

#define OPTION_AUTOCTL_NODENAME(config) \
	make_strbuf_option("pg_autoctl", "nodename", "nodename", \
					   true, _POSIX_HOST_NAME_MAX, \
					   config->nodename)

#define OPTION_POSTGRESQL_PGDATA(config) \
	make_strbuf_option("postgresql", "pgdata", "pgdata", true, MAXPGPATH, \
					   config->pgSetup.pgdata)

#define OPTION_POSTGRESQL_PG_CTL(config) \
	make_strbuf_option("postgresql", "pg_ctl", "pgctl", false, MAXPGPATH, \
					   config->pgSetup.pg_ctl)

#define OPTION_POSTGRESQL_USERNAME(config) \
	make_strbuf_option("postgresql", "username", "username", \
					   false, NAMEDATALEN, \
					   config->pgSetup.username)

#define OPTION_POSTGRESQL_DBNAME(config) \
	make_strbuf_option("postgresql", "dbname", "dbname", false, NAMEDATALEN, \
					   config->pgSetup.dbname)

#define OPTION_POSTGRESQL_HOST(config) \
	make_strbuf_option("postgresql", "host", "pghost", \
					   false, _POSIX_HOST_NAME_MAX, \
					   config->pgSetup.pghost)

#define OPTION_POSTGRESQL_PORT(config) \
	make_int_option("postgresql", "port", "pgport", \
					true, &(config->pgSetup.pgport))

#define OPTION_POSTGRESQL_LISTEN_ADDRESSES(config) \
	make_strbuf_option("postgresql", "listen_addresses", "listen", \
					   false, MAXPGPATH, config->pgSetup.listen_addresses)

#define OPTION_POSTGRESQL_AUTH_METHOD(config) \
	make_strbuf_option("postgresql", "auth_method", "auth", \
					   false, MAXPGPATH, config->pgSetup.authMethod)

#define OPTION_SSL_ACTIVE(config) \
	make_int_option_default("ssl", "active", NULL, \
							false, &(config->pgSetup.ssl.active), 0)

#define OPTION_SSL_MODE(config) \
	make_strbuf_option("ssl", "sslmode", "ssl-mode", \
					   false, SSL_MODE_STRLEN, config->pgSetup.ssl.sslModeStr)

#define OPTION_SSL_CA_FILE(config) \
	make_strbuf_option("ssl", "ca_file", "ssl-ca-file", \
					   false, MAXPGPATH, config->pgSetup.ssl.caFile)

#define OPTION_SSL_CRL_FILE(config) \
	make_strbuf_option("ssl", "crl_file", "ssl-crl-file", \
					   false, MAXPGPATH, config->pgSetup.ssl.crlFile)

#define OPTION_SSL_SERVER_CERT(config) \
	make_strbuf_option("ssl", "cert_file", "server-cert", \
					   false, MAXPGPATH, config->pgSetup.ssl.serverCert)

#define OPTION_SSL_SERVER_KEY(config) \
	make_strbuf_option("ssl", "key_file", "server-key", \
					   false, MAXPGPATH, config->pgSetup.ssl.serverKey)


#define SET_INI_OPTIONS_ARRAY(config) \
	{ \
		OPTION_AUTOCTL_ROLE(config), \
		OPTION_AUTOCTL_NODENAME(config), \
		OPTION_POSTGRESQL_PGDATA(config), \
		OPTION_POSTGRESQL_PG_CTL(config), \
		OPTION_POSTGRESQL_USERNAME(config), \
		OPTION_POSTGRESQL_DBNAME(config), \
		OPTION_POSTGRESQL_HOST(config), \
		OPTION_POSTGRESQL_PORT(config), \
		OPTION_POSTGRESQL_LISTEN_ADDRESSES(config), \
		OPTION_POSTGRESQL_AUTH_METHOD(config), \
		OPTION_SSL_MODE(config), \
		OPTION_SSL_ACTIVE(config), \
		OPTION_SSL_CA_FILE(config), \
		OPTION_SSL_CRL_FILE(config), \
		OPTION_SSL_SERVER_CERT(config), \
		OPTION_SSL_SERVER_KEY(config), \
		INI_OPTION_LAST \
	}


/*
 * monitor_config_set_pathnames_from_pgdata sets the config pathnames from its
 * pgSetup.pgdata field, which must have already been set when calling this
 * function.
 */
bool
monitor_config_set_pathnames_from_pgdata(MonitorConfig *config)
{
	if (IS_EMPTY_STRING_BUFFER(config->pgSetup.pgdata))
	{
		/* developer error */
		log_error("BUG: monitor_config_set_pathnames_from_pgdata: empty pgdata");
		return false;
	}

	if (!SetConfigFilePath(&(config->pathnames), config->pgSetup.pgdata))
	{
		log_fatal("Failed to set configuration filename from PGDATA \"%s\","
				  " see above for details.", config->pgSetup.pgdata);
		return false;
	}

	if (!SetStateFilePath(&(config->pathnames), config->pgSetup.pgdata))
	{
		log_fatal("Failed to set state filename from PGDATA \"%s\","
				  " see above for details.", config->pgSetup.pgdata);
		return false;
	}

	if (!SetPidFilePath(&(config->pathnames), config->pgSetup.pgdata))
	{
		log_fatal("Failed to set pid filename from PGDATA \"%s\","
				  " see above for details.", config->pgSetup.pgdata);
		return false;
	}
	return true;
}


/*
 * monitor_config_init initialises a MonitorConfig with the default values.
 */
void
monitor_config_init(MonitorConfig *config,
					bool missing_pgdata_is_ok, bool pg_is_not_running_is_ok)
{
	PostgresSetup pgSetup = { 0 };
	IniOption monitorOptions[] = SET_INI_OPTIONS_ARRAY(config);

	if (!ini_validate_options(monitorOptions))
	{
		log_error("Please review your setup options per above messages");
		exit(EXIT_CODE_BAD_CONFIG);
	}

	if (!pg_setup_init(&pgSetup,
					   &(config->pgSetup),
					   missing_pgdata_is_ok,
					   pg_is_not_running_is_ok))
	{
		log_error("Please fix your PostgreSQL setup per above messages");
		exit(EXIT_CODE_BAD_CONFIG);
	}

	/*
	 * Keep the whole set of values discovered in pg_setup_init from the
	 * configuration file
	 */
	config->pgSetup = pgSetup;

	/* set our configuration and state file pathnames */
	if (!SetConfigFilePath(&(config->pathnames), config->pgSetup.pgdata))
	{
		log_error("Failed to initialize monitor's config, see above");
		exit(EXIT_CODE_BAD_CONFIG);
	}

	if (!SetStateFilePath(&(config->pathnames), config->pgSetup.pgdata))
	{
		log_error("Failed to initialize monitor's config, see above");
		exit(EXIT_CODE_BAD_CONFIG);
	}

	/* A part of the monitor's pgSetup is hard-coded. */
	strlcpy(config->pgSetup.dbname, PG_AUTOCTL_MONITOR_DBNAME, NAMEDATALEN);
	strlcpy(config->pgSetup.username, PG_AUTOCTL_MONITOR_USERNAME, NAMEDATALEN);
}


/*
 * monitor_config_init initialises a MonitorConfig from a KeeperConfig
 * structure. That's useful for commands that may run on either a monitor or a
 * keeper node, such as `pg_autoctl monitor state|events|formation`, or
 * `pg_autoctl do destroy`.
 */
bool
monitor_config_init_from_pgsetup(MonitorConfig *mconfig,
								 PostgresSetup *pgSetup,
								 bool missingPgdataIsOk,
								 bool pgIsNotRunningIsOk)
{
	PostgresSetup *MpgSetup = &(mconfig->pgSetup);

	/* copy command line options over to the MonitorConfig structure */
	strlcpy(MpgSetup->pgdata, pgSetup->pgdata, MAXPGPATH);
	strlcpy(MpgSetup->pg_ctl, pgSetup->pg_ctl, MAXPGPATH);
	strlcpy(MpgSetup->pg_version, pgSetup->pg_version, PG_VERSION_STRING_MAX);
	strlcpy(MpgSetup->pghost, pgSetup->pghost, _POSIX_HOST_NAME_MAX);
	strlcpy(MpgSetup->listen_addresses, pgSetup->listen_addresses, MAXPGPATH);
	MpgSetup->pgport = pgSetup->pgport;

	/*
	 * Make sure that we keep the SSL options too.
	 */
	MpgSetup->ssl.active = pgSetup->ssl.active;
	MpgSetup->ssl.createSelfSignedCert = pgSetup->ssl.createSelfSignedCert;
	MpgSetup->ssl.sslMode = pgSetup->ssl.sslMode;
	strlcpy(MpgSetup->ssl.sslModeStr, pgSetup->ssl.sslModeStr, SSL_MODE_STRLEN);
	strlcpy(MpgSetup->ssl.caFile, pgSetup->ssl.caFile, MAXPGPATH);
	strlcpy(MpgSetup->ssl.crlFile, pgSetup->ssl.crlFile, MAXPGPATH);
	strlcpy(MpgSetup->ssl.serverCert, pgSetup->ssl.serverCert, MAXPGPATH);
	strlcpy(MpgSetup->ssl.serverKey, pgSetup->ssl.serverKey, MAXPGPATH);

	if (!monitor_config_set_pathnames_from_pgdata(mconfig))
	{
		/* errors have already been logged */
		return false;
	}

	if (!monitor_config_read_file(mconfig,
								  missingPgdataIsOk,
								  pgIsNotRunningIsOk))
	{
		log_fatal("Failed to read configuration file \"%s\"",
				  mconfig->pathnames.config);
		return false;
	}

	return true;
}


/*
 * monitor_config_read_file overrides values in given MonitorConfig with
 * whatever values are read from given configuration filename.
 */
bool
monitor_config_read_file(MonitorConfig *config,
						 bool missing_pgdata_is_ok,
						 bool pg_not_running_is_ok)
{
	const char *filename = config->pathnames.config;
	PostgresSetup pgSetup = { 0 };
	IniOption monitorOptions[] = SET_INI_OPTIONS_ARRAY(config);

	log_debug("Reading configuration from %s", filename);

	if (!read_ini_file(filename, monitorOptions))
	{
		log_error("Failed to parse configuration file \"%s\"", filename);
		return false;
	}

	if (!pg_setup_init(&pgSetup,
					   &config->pgSetup,
					   missing_pgdata_is_ok,
					   pg_not_running_is_ok))
	{
		return false;
	}

	/*
	 * Keep the whole set of values discovered in pg_setup_init from the
	 * configuration file
	 */
	config->pgSetup = pgSetup;

	/* A part of the monitor's pgSetup is hard-coded. */
	strlcpy(config->pgSetup.dbname, PG_AUTOCTL_MONITOR_DBNAME, NAMEDATALEN);
	strlcpy(config->pgSetup.username, PG_AUTOCTL_MONITOR_USERNAME, NAMEDATALEN);

	/* set the ENUM value for sslMode */
	config->pgSetup.ssl.sslMode =
		pgsetup_parse_sslmode(config->pgSetup.ssl.sslModeStr);

	return true;
}


/*
 * monitor_config_write_file writes the current values in given KeeperConfig to
 * filename.
 */
bool
monitor_config_write_file(MonitorConfig *config)
{
	const char *filePath = config->pathnames.config;
	bool success = false;
	FILE *fileStream = NULL;

	log_trace("monitor_config_write_file \"%s\"", filePath);

	fileStream = fopen_with_umask(filePath, "w", FOPEN_FLAGS_W, 0644);
	if (fileStream == NULL)
	{
		/* errors have already been logged */
		return false;
	}

	success = monitor_config_write(fileStream, config);

	if (fclose(fileStream) == EOF)
	{
		log_error("Failed to write file \"%s\"", filePath);
		return false;
	}

	return success;
}


/*
 * monitor_config_write write the current config to given STREAM.
 */
bool
monitor_config_write(FILE *stream, MonitorConfig *config)
{
	IniOption monitorOptions[] = SET_INI_OPTIONS_ARRAY(config);

	return write_ini_to_stream(stream, monitorOptions);
}


/*
 * monitor_config_to_json populates given jsRoot object with the INI
 * configuration sections as JSON objects, and the options as keys to those
 * objects.
 */
bool
monitor_config_to_json(MonitorConfig *config, JSON_Value *js)
{
	JSON_Object *jsRoot = json_value_get_object(js);
	IniOption monitorOptions[] = SET_INI_OPTIONS_ARRAY(config);

	return ini_to_json(jsRoot, monitorOptions);
}


/*
 * monitor_config_log_settings outputs a DEBUG line per each config parameter
 * in the given MonitorConfig.
 */
void
monitor_config_log_settings(MonitorConfig config)
{
	log_debug("postgresql.pgdata: %s", config.pgSetup.pgdata);
	log_debug("postgresql.pg_ctl: %s", config.pgSetup.pg_ctl);
	log_debug("postgresql.version: %s", config.pgSetup.pg_version);
	log_debug("postgresql.username: %s", config.pgSetup.username);
	log_debug("postgresql.dbname: %s", config.pgSetup.dbname);
	log_debug("postgresql.host: %s", config.pgSetup.pghost);
	log_debug("postgresql.port: %d", config.pgSetup.pgport);
	log_debug("postgresql.auth: %s", config.pgSetup.authMethod);

	log_debug("ssl.active: %d", config.pgSetup.ssl.active);
	log_debug("ssl.sslMode: %s", config.pgSetup.ssl.sslModeStr);
	log_debug("ssl.caFile: %s", config.pgSetup.ssl.caFile);
	log_debug("ssl.crlFile: %s", config.pgSetup.ssl.crlFile);
	log_debug("ssl.serverKey: %s", config.pgSetup.ssl.serverCert);
	log_debug("ssl.serverCert: %s", config.pgSetup.ssl.serverKey);
}


/*
 * monitor_config_merge_options merges any option setup in options into config.
 * Its main use is to override configuration file settings with command line
 * options.
 */
bool
monitor_config_merge_options(MonitorConfig *config, MonitorConfig *options)
{
	IniOption monitorConfigOptions[] = SET_INI_OPTIONS_ARRAY(config);
	IniOption monitorOptionsOptions[] = SET_INI_OPTIONS_ARRAY(options);

	if (ini_merge(monitorConfigOptions, monitorOptionsOptions))
	{
		PostgresSetup pgSetup = { 0 };
		bool missing_pgdata_is_ok = true;
		bool pg_is_not_running_is_ok = true;

		/*
		 * Before merging given options, validate them as much as we can. The
		 * ini level functions validate the syntax (strings, integers, etc),
		 * not that the values themselves then make sense.
		 */
		if (!pg_setup_init(&pgSetup,
						   &config->pgSetup,
						   missing_pgdata_is_ok,
						   pg_is_not_running_is_ok))
		{
			return false;
		}

		/*
		 * Keep the whole set of values discovered in pg_setup_init from the
		 * configuration file
		 */
		config->pgSetup = pgSetup;

		return monitor_config_write_file(config);
	}
	return false;
}


/*
 * monitor_config_get_postgres_uri build a connecting string to connect
 * to the monitor server from a remote machine and writes it to connectionString,
 * with at most size number of chars.
 */
bool
monitor_config_get_postgres_uri(MonitorConfig *config, char *connectionString,
								size_t size)
{
	char *connStringEnd = connectionString;
	char host[BUFSIZE];

	if (!IS_EMPTY_STRING_BUFFER(config->nodename))
	{
		strlcpy(host, config->nodename, BUFSIZE);
	}
	else if (IS_EMPTY_STRING_BUFFER(config->pgSetup.listen_addresses) ||
			 strcmp(config->pgSetup.listen_addresses,
					POSTGRES_DEFAULT_LISTEN_ADDRESSES) == 0)
	{
		/*
		 * We ouput the monitor connection string using the LAN ip of the
		 * current machine (e.g. 192.168.1.1), which is the most probable IP
		 * address that the other members of the pg_auto_failover cluster will
		 * have to use to register and communicate with the monitor.
		 *
		 * The monitor_install() function also has added an HBA entry to this
		 * PostgreSQL server to open it up to the local area network, e.g.
		 * 129.168.1.0/23, so it should just work here.
		 */
		if (!fetchLocalIPAddress(host, BUFSIZE,
								 DEFAULT_INTERFACE_LOOKUP_SERVICE_NAME,
								 DEFAULT_INTERFACE_LOOKUP_SERVICE_PORT))
		{
			/* error is already logged */
			return false;
		}
	}
	else
	{
		strlcpy(host, config->pgSetup.listen_addresses, BUFSIZE);
	}

	/*
	 * Finalize the connection string, with some variants depending on the
	 * usage of SSL certificates. The full variant is with sslrootcert and
	 * sslcrl connection parameters when using sslmode=verify-ca or
	 * sslmode=verify-full.
	 */
	connStringEnd += sformat(connStringEnd,
							 size - (connStringEnd - connectionString),
							 "postgres://%s@%s:%d/%s",
							 config->pgSetup.username,
							 host,
							 config->pgSetup.pgport,
							 config->pgSetup.dbname);

	if (config->pgSetup.ssl.sslMode >= SSL_MODE_PREFER)
	{
		char *sslmode = pgsetup_sslmode_to_string(config->pgSetup.ssl.sslMode);

		connStringEnd += sformat(connStringEnd,
								 size - (connStringEnd - connectionString),
								 "?sslmode=%s",
								 sslmode);

		if (config->pgSetup.ssl.sslMode >= SSL_MODE_VERIFY_CA)
		{
			if (IS_EMPTY_STRING_BUFFER(config->pgSetup.ssl.crlFile))
			{
				connStringEnd +=
					sformat(connStringEnd,
							size - (connStringEnd - connectionString),
							"&sslrootcert=%s",
							config->pgSetup.ssl.caFile);
			}
			else
			{
				connStringEnd +=
					sformat(connStringEnd,
							size - (connStringEnd - connectionString),
							"&sslrootcert=%s&sslcrl=%s",
							config->pgSetup.ssl.caFile,
							config->pgSetup.ssl.crlFile);
			}
		}
	}

	return true;
}


/*
 * monitor_config_get_setting returns the current value of the given option
 * "path" (thats a section.option string). The value is returned in the
 * pre-allocated value buffer of size size.
 */
bool
monitor_config_get_setting(MonitorConfig *config,
						   const char *path,
						   char *value, size_t size)
{
	const char *filename = config->pathnames.config;
	IniOption monitorOptions[] = SET_INI_OPTIONS_ARRAY(config);

	return ini_get_setting(filename, monitorOptions, path, value, size);
}


/*
 * monitor_config_set_setting sets the setting identified by "path"
 * (section.option) to the given value. The value is passed in as a string,
 * which is going to be parsed if necessary.
 */
bool
monitor_config_set_setting(MonitorConfig *config,
						   const char *path,
						   char *value)
{
	const char *filename = config->pathnames.config;
	IniOption monitorOptions[] = SET_INI_OPTIONS_ARRAY(config);

	if (ini_set_setting(filename, monitorOptions, path, value))
	{
		PostgresSetup pgSetup = { 0 };
		bool missing_pgdata_is_ok = true;
		bool pg_is_not_running_is_ok = true;

		/*
		 * Before merging given options, validate them as much as we can. The
		 * ini level functions validate the syntax (strings, integers, etc),
		 * not that the values themselves then make sense.
		 */
		return pg_setup_init(&pgSetup,
							 &(config->pgSetup),
							 missing_pgdata_is_ok,
							 pg_is_not_running_is_ok);
	}

	return false;
}


/*
 * monitor_config_update_with_absolute_pgdata verifies that the pgdata path
 * is an absolute one
 * If not, the config->pgSetup is updated and we rewrite the monitor config file
 */
bool
monitor_config_update_with_absolute_pgdata(MonitorConfig *config)
{
	PostgresSetup *pgSetup = &(config->pgSetup);

	if (pg_setup_set_absolute_pgdata(pgSetup))
	{
		if (!monitor_config_write_file(config))
		{
			/* errors have already been logged */
			return false;
		}
	}

	return true;
}
