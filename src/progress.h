#ifndef BETTER_PDF_PROGRESS_H
#define BETTER_PDF_PROGRESS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Progress accounting and display for one run.
 *
 * The page loop reports how long each page took; the resulting running
 * average turns the pages that are left into an ETA. Formatting is separate
 * from the accounting so the line can be produced without touching a clock or
 * a log sink and can be checked against exact strings.
 *
 * The display keeps one line at the bottom of a console: it is redrawn in
 * place with a carriage return while a page is in flight, cleared before a log
 * line is written, and redrawn after. When the stream is not a console the
 * display is disabled and the caller falls back to ordinary log lines.
 */

typedef struct {
    uint32_t total_pages;
    uint32_t completed_pages;
    double page_seconds;
} progress;

typedef struct {
    FILE *stream;
    char line[192];
    uint32_t painted;
    bool visible;
} progress_display;

void progress_init(progress *value, uint32_t total_pages);
void progress_add_page(progress *value, double seconds);
double progress_average_seconds(const progress *value);
double progress_eta_seconds(const progress *value);

/* Writes "[###......] 3/12 pages, 25% | 10.0 s/page | ETA 1m 30s" and returns
 * its length, or 0 when the result does not fit. */
uint64_t progress_format(const progress *value, char *output, uint64_t capacity);

/* The same line with the page that is currently in flight, e.g.
 * "[###......] 3/12 pages, 25% | page 4: 17s | 10.0 s/page | ETA 1m 30s". */
uint64_t progress_format_active(const progress *value, uint32_t page_number, double page_seconds, char *output, uint64_t capacity);

/* A NULL stream disables the display: every drawing call becomes a no-op. */
void progress_display_init(progress_display *display, FILE *stream);
bool progress_display_enabled(const progress_display *display);
void progress_display_show(progress_display *display, const char *line);
void progress_display_suspend(progress_display *display);
void progress_display_resume(progress_display *display);
void progress_display_complete(progress_display *display, const char *line);

#endif