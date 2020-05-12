/*
 * src/bin/pg_autoctl/service_postgres.h
 *   Utilities to start/stop the pg_autoctl service on a monitor node.
 *
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the PostgreSQL License.
 *
 */
#ifndef SERVICE_POSTGRES_H
#define SERVICE_POSTGRES_H

#include <inttypes.h>
#include <signal.h>

#include "keeper.h"
#include "keeper_config.h"

extern int countPostgresStart;

bool service_postgres_start(void *context, pid_t *pid);
bool service_postgres_stop(void *context);
void service_postgres_reload(void *context);

#endif /* SERVICE_POSTGRES_H */
