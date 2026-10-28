#ifndef BETTER_PDF_PLATFORM_WIN32_H
#define BETTER_PDF_PLATFORM_WIN32_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Thin Win32 adapters. This layer contains no policy decisions: every failure
 * is reported back to the caller, which decides what to do and logs it.
 */

double platform_monotonic_seconds(void);

/* Reads a whole file. Returns false on an I/O error or when the file is larger
 * than capacity. A missing file is reported as found = false and true. */
bool platform_read_file(const char *path, char *buffer, uint64_t capacity, uint64_t *length, bool *found);
bool platform_write_file(const char *path, const char *data, uint64_t length);
bool platform_create_directories(const char *path);
bool platform_path_exists(const char *path);
bool platform_copy_file(const char *from_path, const char *to_path);
bool platform_remove_file(const char *path);
bool platform_remove_directory(const char *path);

/* Replaces GetCommandLineW()'s arguments as UTF-8. Returns the argument count,
 * or 0 when conversion fails or the storage is too small. */
uint32_t platform_utf8_arguments(const char **arguments, char *storage, uint64_t storage_capacity);

void platform_enable_utf8_console(void);

#endif