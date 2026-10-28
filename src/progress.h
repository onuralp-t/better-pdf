#ifndef BETTER_PDF_PROGRESS_H
#define BETTER_PDF_PROGRESS_H

#include <stdint.h>

/*
 * Progress accounting for one run.
 *
 * The page loop reports how long each page took; the resulting running
 * average turns the pages that are left into an ETA. Formatting is separate
 * from the accounting so the line can be produced without touching a clock or
 * a log sink and can be checked against exact strings.
 */

typedef struct {
    uint32_t total_pages;
    uint32_t completed_pages;
    double page_seconds;
} progress;

void progress_init(progress *value, uint32_t total_pages);
void progress_add_page(progress *value, double seconds);
double progress_average_seconds(const progress *value);
double progress_eta_seconds(const progress *value);

/* Writes "[###......] 3/12 pages, 25% | 10.0 s/page | ETA 1m 30s" and returns
 * its length, or 0 when the result does not fit. */
uint64_t progress_format(const progress *value, char *output, uint64_t capacity);

#endif