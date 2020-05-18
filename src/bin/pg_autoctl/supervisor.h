/*
 * src/bin/pg_autoctl/supervisor.h
 *   Utilities to start/stop the pg_autoctl services.
 *
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the PostgreSQL License.
 *
 */
#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include <inttypes.h>
#include <signal.h>

/*
 * Our supervisor process may retart a service sub-process when it quits,
 * depending on the exit status and the restart policy that has been choosen:
 *
 * - A permanent child process is always restarted.
 *
 * - A temporary child process is never restarted.
 *
 * - A transient child process is restarted only if it terminates abnormally,
 *   that is, with an exit code other EXIT_CODE_QUIT (zero).
 */
typedef enum
{
	RP_PERMANENT = 0,
	RP_TEMPORARY,
	RP_TRANSIENT
} RestartPolicy;

/*
 * The supervisor works with an array of Service entries. Each service defines
 * its behavior thanks to a start function, a stop function, and a reload
 * function. Those are called at different points to adjust to the situation as
 * seen by the supervisor.
 *
 * In particular, services may be started more than once when they fail.
 */

typedef struct Service
{
	char name[NAMEDATALEN];             /* Service name for the user */
	RestartPolicy policy;               /* Should we restart the service? */
	pid_t pid;                          /* Service PID */
	bool (*startFunction)(void *context, pid_t *pid);
	void *context;             /* Service Context (Monitor or Keeper struct) */
	int retries;
	uint64_t startTime;
	uint64_t stopTime;
} Service;

typedef struct Supervisor
{
	Service *services;
	int serviceCount;
	char pidfile[MAXPGPATH];
	pid_t pid;
	bool cleanExit;
	bool shutdownSequenceInProgress;
} Supervisor;


bool supervisor_start(Service services[], int serviceCount, const char *pidfile);

bool supervisor_stop(Supervisor *supervisor);

bool supervisor_find_service_pid(const char *pidfile,
								 const char *serviceName,
								 pid_t *pid);


#endif /* SUPERVISOR_H */
