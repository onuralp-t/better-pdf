#ifndef BETTER_PDF_LOG_H
#define BETTER_PDF_LOG_H

#include <stdint.h>
#include <stdio.h>

typedef enum {
    LOG_LEVEL_INFO = 0,
    LOG_LEVEL_WARN = 1,
    LOG_LEVEL_ERROR = 2
} log_level;

void log_init(FILE *sink);
void log_set_sink(FILE *sink);
void log_emit(log_level level, const char *file, uint32_t line, const char *format, ...);

#define LOG(...) log_emit(LOG_LEVEL_INFO, __FILE__, (uint32_t)__LINE__, __VA_ARGS__)
#define WARN(...) log_emit(LOG_LEVEL_WARN, __FILE__, (uint32_t)__LINE__, __VA_ARGS__)
#define ERROR(...) log_emit(LOG_LEVEL_ERROR, __FILE__, (uint32_t)__LINE__, __VA_ARGS__)

#endif