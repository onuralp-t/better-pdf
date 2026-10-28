#include "progress.h"

#include "asserts.h"

#include <stdio.h>

#define PROGRESS_BAR_WIDTH 24u
#define PROGRESS_DURATION_CAPACITY 32u

void progress_init(progress *value, uint32_t total_pages) {
    ASSERT(value != NULL);
    ASSERT(total_pages > 0u);
    value->total_pages = total_pages;
    value->completed_pages = 0u;
    value->page_seconds = 0.0;
}

void progress_add_page(progress *value, double seconds) {
    ASSERT(value != NULL);
    ASSERT(seconds >= 0.0);
    ASSERT(value->completed_pages < value->total_pages);
    value->completed_pages += 1u;
    value->page_seconds += seconds;
}

double progress_average_seconds(const progress *value) {
    ASSERT(value != NULL);
    if (value->completed_pages == 0u) {
        return 0.0;
    }
    return value->page_seconds / (double)value->completed_pages;
}

double progress_eta_seconds(const progress *value) {
    ASSERT(value != NULL);
    uint32_t remaining_pages = value->total_pages - value->completed_pages;
    return (double)remaining_pages * progress_average_seconds(value);
}

static uint32_t progress_bar_filled(const progress *value) {
    return (uint32_t)(((uint64_t)value->completed_pages * PROGRESS_BAR_WIDTH) / value->total_pages);
}

static uint32_t progress_percent(const progress *value) {
    return (uint32_t)(((uint64_t)value->completed_pages * 100u) / value->total_pages);
}

static void progress_format_duration(uint64_t seconds, char *output, uint64_t capacity) {
    if (seconds >= 3600u) {
        (void)snprintf(output, (size_t)capacity, "%lluh %02llum",
                       (unsigned long long)(seconds / 3600u),
                       (unsigned long long)((seconds % 3600u) / 60u));
        return;
    }
    if (seconds >= 60u) {
        (void)snprintf(output, (size_t)capacity, "%llum %02llus",
                       (unsigned long long)(seconds / 60u),
                       (unsigned long long)(seconds % 60u));
        return;
    }
    (void)snprintf(output, (size_t)capacity, "%llus", (unsigned long long)seconds);
}

uint64_t progress_format(const progress *value, char *output, uint64_t capacity) {
    ASSERT(value != NULL);
    ASSERT(output != NULL);
    ASSERT(capacity > 0u);

    /* Round the ETA up: a run with work left never claims zero seconds. */
    double eta_seconds = progress_eta_seconds(value);
    uint64_t whole_seconds = (uint64_t)eta_seconds;
    if ((double)whole_seconds < eta_seconds) {
        whole_seconds += 1u;
    }
    char eta[PROGRESS_DURATION_CAPACITY];
    progress_format_duration(whole_seconds, eta, sizeof eta);

    char bar[PROGRESS_BAR_WIDTH + 1u];
    uint32_t filled = progress_bar_filled(value);
    for (uint32_t index = 0; index < PROGRESS_BAR_WIDTH; index += 1u) {
        bar[index] = index < filled ? '#' : '.';
    }
    bar[PROGRESS_BAR_WIDTH] = '\0';

    int written = snprintf(output, (size_t)capacity, "[%s] %u/%u pages, %u%% | %.1f s/page | ETA %s",
                           bar, value->completed_pages, value->total_pages, progress_percent(value),
                           progress_average_seconds(value), eta);
    ASSERT(written >= 0);
    /* A negative snprintf result wraps to a huge uint64 and fails the fit. */
    uint64_t length = (uint64_t)written;
    if (length >= capacity) {
        return 0u;
    }
    return length;
}