#include "log.h"

#include "asserts.h"
#include "platform_win32.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#define LOG_MESSAGE_CAPACITY 1024u

static const char *const log_level_names[] = { "LOG", "WARN", "ERROR" };

static FILE *log_sink_file = NULL;
static double log_started_seconds = 0.0;
static progress_display *log_status_display = NULL;

void log_init(FILE *sink) {
    ASSERT(sink != NULL);
    log_sink_file = sink;
    log_started_seconds = platform_monotonic_seconds();
}

void log_set_sink(FILE *sink) {
    ASSERT(sink != NULL);
    log_sink_file = sink;
}

void log_set_status(progress_display *display) {
    log_status_display = display;
}

bool log_ready(void) {
    return log_sink_file != NULL;
}

void log_emit(log_level level, const char *file, uint32_t line, const char *format, ...) {
    ASSERT(log_sink_file != NULL);
    ASSERT(format != NULL);
    ASSERT((uint32_t)level <= (uint32_t)LOG_LEVEL_ERROR);

    char message[LOG_MESSAGE_CAPACITY];
    va_list arguments;
    va_start(arguments, format);
    int written = vsnprintf(message, sizeof message, format, arguments);
    va_end(arguments);
    ASSERT(written >= 0);

    double elapsed_seconds = platform_monotonic_seconds() - log_started_seconds;
    if (log_status_display != NULL) {
        progress_display_suspend(log_status_display);
    }
    if (level == LOG_LEVEL_ERROR) {
        fprintf(log_sink_file, "[%6.1fs] ERROR %s:%u: %s\n", elapsed_seconds, file, line, message);
    } else {
        fprintf(log_sink_file, "[%6.1fs] %-5s %s\n", elapsed_seconds, log_level_names[level], message);
    }
    fflush(log_sink_file);
    if (log_status_display != NULL) {
        progress_display_resume(log_status_display);
    }
}