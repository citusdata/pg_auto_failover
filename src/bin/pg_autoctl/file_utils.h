/*
 * src/bin/pg_autoctl/file_utils.h
 *   Utility functions for reading and writing files
 *
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the PostgreSQL License.
 *
 */

#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stdarg.h>

#include "postgres_fe.h"

#include <fcntl.h>


bool file_exists(const char *filename);
bool directory_exists(const char *path);
bool ensure_empty_dir(const char *dirname, int mode);
FILE * fopen_with_umask(const char *filePath, const char* modes, int flags, mode_t umask);
bool write_file(char *data, long fileSize, const char *filePath);
bool append_to_file(char *data, long fileSize, const char *filePath);
bool read_file(const char *filePath, char **contents, long *fileSize);
bool move_file(char* sourcePath, char* destinationPath);
bool duplicate_file(char* sourcePath, char* destinationPath);
bool create_symbolic_link(char* sourcePath, char* targetPath);

void path_in_same_directory(const char *basePath,
							const char *fileName,
							char *destinationPath);

int search_pathlist(const char *pathlist, const char *filename, char ***result);
void search_pathlist_destroy_result(char **result);
bool unlink_file(const char *filename);
bool set_program_absolute_path(char *program, int size);
bool normalize_filename(const char *filename, char *dst, int size);

int fformat(FILE *stream, const char *fmt, ...)
 	__attribute__((format(printf, 2, 3)));

#endif /* FILE_UTILS_H */
