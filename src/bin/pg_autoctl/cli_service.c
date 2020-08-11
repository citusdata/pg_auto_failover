/*
 * src/bin/pg_autoctl/cli_service.c
 *     Implementation of a CLI for controlling the pg_autoctl service.
 *
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the PostgreSQL License.
 *
 */

#include <errno.h>
#include <inttypes.h>
#include <getopt.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "postgres_fe.h"

#include "cli_common.h"
#include "commandline.h"
#include "defaults.h"
#include "fsm.h"
#include "keeper_config.h"
#include "keeper.h"
#include "monitor.h"
#include "monitor_config.h"
#include "pidfile.h"
#include "service_keeper.h"
#include "service_monitor.h"
#include "signals.h"

static int stop_signal = SIGTERM;

static void cli_service_run(int argc, char **argv);
static void cli_keeper_run(int argc, char **argv);
static void cli_monitor_run(int argc, char **argv);

static int cli_getopt_pgdata_and_mode(int argc, char **argv);

static void cli_service_stop(int argc, char **argv);
static void cli_service_reload(int argc, char **argv);
static void cli_service_status(int argc, char **argv);

CommandLine service_run_command =
	make_command("run",
				 "Run the pg_autoctl service (monitor or keeper)",
				 " [ --pgdata --nodename --hostname --pgport ] ",
				 "  --pgdata      path to data directory\n"
				 "  --nodename    pg_auto_failover node name\n"
				 "  --hostname    hostname used to connect from other nodes\n"
				 "  --pgport      PostgreSQL's port number\n",
				 cli_node_metadata_getopts,
				 cli_service_run);

CommandLine service_stop_command =
	make_command("stop",
				 "signal the pg_autoctl service for it to stop",
				 " [ --pgdata --fast --immediate ]",
				 "  --pgdata      path to data director \n"
				 "  --fast        fast shutdown mode for the keeper \n"
				 "  --immediate   immediate shutdown mode for the keeper \n",
				 cli_getopt_pgdata_and_mode,
				 cli_service_stop);

CommandLine service_reload_command =
	make_command("reload",
				 "signal the pg_autoctl for it to reload its configuration",
				 CLI_PGDATA_USAGE,
				 CLI_PGDATA_OPTION,
				 cli_getopt_pgdata,
				 cli_service_reload);

CommandLine service_status_command =
	make_command("status",
				 "Display the current status of the pg_autoctl service",
				 CLI_PGDATA_USAGE,
				 CLI_PGDATA_OPTION,
				 cli_getopt_pgdata,
				 cli_service_status);


/*
 * cli_service_run starts the local pg_auto_failover service, either the
 * monitor or the keeper, depending on the configuration file associated with
 * the current PGDATA, or the --pgdata argument.
 */
static void
cli_service_run(int argc, char **argv)
{
	KeeperConfig config = keeperOptions;

	if (!keeper_config_set_pathnames_from_pgdata(&config.pathnames,
												 config.pgSetup.pgdata))
	{
		/* errors have already been logged */
		exit(EXIT_CODE_BAD_CONFIG);
	}

	switch (ProbeConfigurationFileRole(config.pathnames.config))
	{
		case PG_AUTOCTL_ROLE_MONITOR:
		{
			(void) cli_monitor_run(argc, argv);
			break;
		}

		case PG_AUTOCTL_ROLE_KEEPER:
		{
			(void) cli_keeper_run(argc, argv);
			break;
		}

		default:
		{
			log_fatal("Unrecognized configuration file \"%s\"",
					  config.pathnames.config);
			exit(EXIT_CODE_INTERNAL_ERROR);
		}
	}
}


/*
 * keeper_cli_fsm_run runs the keeper state machine in an infinite
 * loop.
 */
static void
cli_keeper_run(int argc, char **argv)
{
	Keeper keeper = { 0 };
	Monitor *monitor = &(keeper.monitor);
	KeeperConfig *config = &(keeper.config);
	PostgresSetup *pgSetup = &(keeper.config.pgSetup);
	LocalPostgresServer *postgres = &(keeper.postgres);

	/* in case --name, --hostname, or --pgport are used */
	KeeperConfig oldConfig = { 0 };

	bool missingPgdataIsOk = true;
	bool pgIsNotRunningIsOk = true;
	bool monitorDisabledIsOk = true;

	keeper.config = keeperOptions;

	/* initialize our pgSetup and LocalPostgresServer instances */
	if (!keeper_config_read_file(config,
								 missingPgdataIsOk,
								 pgIsNotRunningIsOk,
								 monitorDisabledIsOk))
	{
		/* errors have already been logged. */
		exit(EXIT_CODE_BAD_CONFIG);
	}

	/* keep a copy */
	oldConfig = *config;

	/*
	 * Now that we have loaded the configuration file, apply the command
	 * line options on top of it, giving them priority over the config.
	 */
	if (!keeper_config_merge_options(config, &keeperOptions))
	{
		/* errors have been logged already */
		exit(EXIT_CODE_BAD_CONFIG);
	}

	if (!monitor_init(monitor, config->monitor_pguri))
	{
		/* errors have already been logged */
		exit(EXIT_CODE_BAD_ARGS);
	}

	if (keeper_set_node_metadata(&keeper, &oldConfig))
	{
		/* we don't keep a connection to the monitor in this process */
		pgsql_finish(&(monitor->pgsql));
	}
	else
	{
		/* errors have already been logged */
		exit(EXIT_CODE_MONITOR);
	}

	/* initialize our local Postgres instance representation */
	(void) local_postgres_init(postgres, pgSetup);

	if (!start_keeper(&keeper))
	{
		log_fatal("Failed to start pg_autoctl keeper service, "
				  "see above for details");
		exit(EXIT_CODE_INTERNAL_ERROR);
	}
}


/*
 * cli_monitor_run ensures PostgreSQL is running and then listens for state
 * changes from the monitor, logging them as INFO messages. Also listens for
 * log messages from the monitor, and outputs them as DEBUG messages.
 */
static void
cli_monitor_run(int argc, char **argv)
{
	KeeperConfig options = keeperOptions;

	Monitor monitor = { 0 };
	bool missingPgdataIsOk = false;
	bool pgIsNotRunningIsOk = true;

	/* Prepare MonitorConfig from the CLI options fed in options */
	if (!monitor_config_init_from_pgsetup(&(monitor.config),
										  &options.pgSetup,
										  missingPgdataIsOk,
										  pgIsNotRunningIsOk))
	{
		/* errors have already been logged */
		exit(EXIT_CODE_PGCTL);
	}

	/* Start the monitor service */
	if (!start_monitor(&monitor))
	{
		log_fatal("Failed to start pg_autoctl monitor service, "
				  "see above for details");
		exit(EXIT_CODE_INTERNAL_ERROR);
	}
}


/*
 * service_cli_reload sends a SIGHUP signal to the keeper.
 */
static void
cli_service_reload(int argc, char **argv)
{
	pid_t pid;
	Keeper keeper = { 0 };

	keeper.config = keeperOptions;

	if (read_pidfile(keeper.config.pathnames.pid, &pid))
	{
		if (kill(pid, SIGHUP) != 0)
		{
			log_error("Failed to send SIGHUP to pg_autoctl pid %d: %m", pid);
			exit(EXIT_CODE_INTERNAL_ERROR);
		}
	}
}


/*
 * cli_getopt_pgdata_and_mode gets both the --pgdata and the stopping mode
 * options (either --fast or --immediate) from the command line.
 */
static int
cli_getopt_pgdata_and_mode(int argc, char **argv)
{
	KeeperConfig options = { 0 };
	int c, option_index = 0;
	int verboseCount = 0;

	static struct option long_options[] = {
		{ "pgdata", required_argument, NULL, 'D' },
		{ "fast", no_argument, NULL, 'f' },
		{ "immediate", no_argument, NULL, 'i' },
		{ "version", no_argument, NULL, 'V' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "quiet", no_argument, NULL, 'q' },
		{ "help", no_argument, NULL, 'h' },
		{ NULL, 0, NULL, 0 }
	};

	optind = 0;

	while ((c = getopt_long(argc, argv, "D:fiVvqh",
							long_options, &option_index)) != -1)
	{
		switch (c)
		{
			case 'D':
			{
				strlcpy(options.pgSetup.pgdata, optarg, MAXPGPATH);
				log_trace("--pgdata %s", options.pgSetup.pgdata);
				break;
			}

			case 'f':
			{
				/* change the signal to send from SIGTERM to SIGINT. */
				if (stop_signal != SIGTERM)
				{
					log_fatal("Please use either --fast or --immediate, not both");
					exit(EXIT_CODE_BAD_ARGS);
				}
				stop_signal = SIGINT;
				break;
			}

			case 'i':
			{
				/* change the signal to send from SIGTERM to SIGQUIT. */
				if (stop_signal != SIGTERM)
				{
					log_fatal("Please use either --fast or --immediate, not both");
					exit(EXIT_CODE_BAD_ARGS);
				}
				stop_signal = SIGQUIT;
				break;
			}

			case 'V':
			{
				/* keeper_cli_print_version prints version and exits. */
				keeper_cli_print_version(argc, argv);
				break;
			}

			case 'v':
			{
				++verboseCount;
				switch (verboseCount)
				{
					case 1:
					{
						log_set_level(LOG_INFO);
						break;
					}

					case 2:
					{
						log_set_level(LOG_DEBUG);
						break;
					}

					default:
					{
						log_set_level(LOG_TRACE);
						break;
					}
				}
				break;
			}

			case 'q':
			{
				log_set_level(LOG_ERROR);
				break;
			}

			case 'h':
			{
				commandline_help(stderr);
				exit(EXIT_CODE_QUIT);
				break;
			}

			default:
			{
				commandline_help(stderr);
				exit(EXIT_CODE_BAD_ARGS);
				break;
			}
		}
	}

	/* now that we have the command line parameters, prepare the options */
	(void) prepare_keeper_options(&options);

	keeperOptions = options;

	return optind;
}


/*
 * cli_service_stop sends a SIGTERM signal to the keeper.
 */
static void
cli_service_stop(int argc, char **argv)
{
	pid_t pid;
	Keeper keeper = { 0 };

	keeper.config = keeperOptions;

	if (read_pidfile(keeper.config.pathnames.pid, &pid))
	{
		if (kill(pid, stop_signal) != 0)
		{
			log_error("Failed to send %s to pg_autoctl pid %d: %m",
					  strsignal(stop_signal), pid);
			exit(EXIT_CODE_INTERNAL_ERROR);
		}
	}
	else
	{
		log_fatal("Failed to read the keeper's PID at \"%s\"",
				  keeper.config.pathnames.pid);
	}
}


/*
 * cli_service_status displays the status of the pg_autoctl service and the
 * Postgres service.
 */
static void
cli_service_status(int argc, char **argv)
{
	pid_t pid = 0;
	Keeper keeper = { 0 };
	PostgresSetup *pgSetup = &(keeper.config.pgSetup);
	ConfigFilePaths *pathnames = &(keeper.config.pathnames);

	keeper.config = keeperOptions;

	if (!cli_common_pgsetup_init(pathnames, pgSetup))
	{
		/* errors have already been logged */
		exit(EXIT_CODE_BAD_CONFIG);
	}

	if (!file_exists(pathnames->pid))
	{
		log_debug("pg_autoctl pid file \"%s\" does not exists", pathnames->pid);

		if (pg_setup_is_running(pgSetup))
		{
			log_fatal("Postgres is running at \"%s\" with pid %d",
					  pgSetup->pgdata, pgSetup->pidFile.pid);
		}

		log_info("pg_autoctl is not running at \"%s\"", pgSetup->pgdata);
		exit(PG_CTL_STATUS_NOT_RUNNING);
	}

	/* ok now we have a pidfile for pg_autoctl */
	if (read_pidfile(pathnames->pid, &pid) && pid > 0)
	{
		if (kill(pid, 0) != 0)
		{
			log_error("pg_autoctl pid file contains stale pid %d", pid);
			exit(PG_CTL_STATUS_NOT_RUNNING);
		}
	}

	/* and now we know pg_autoctl is running */
	if (pid > 0)
	{
		log_info("pg_autoctl is running with pid %d", pid);
	}

	/* add a word about the Postgres service itself */
	if (pg_setup_is_ready(pgSetup, false))
	{
		log_info("Postgres is serving PGDATA \"%s\" on port %d with pid %d",
				 pgSetup->pgdata, pgSetup->pgport, pgSetup->pidFile.pid);
	}
	else
	{
		exit(EXIT_CODE_PGCTL);
	}

	if (outputJSON)
	{
		bool includeStatus = true;

		(void) fprint_pidfile_as_json(pathnames->pid, includeStatus);
	}
}
