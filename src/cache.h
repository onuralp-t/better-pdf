#ifndef BETTER_PDF_CACHE_H
#define BETTER_PDF_CACHE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Per-page OCR cache.
 *
 * OCR is the expensive step (tens of seconds per page), so a page's text is
 * written to disk as soon as it arrives. The cache directory is named after a
 * hash of everything that could change the result: pipeline version, input
 * path and size, page count, pixel budget, and prompt. Changing any of those
 * produces a different directory, so stale text can never be mixed into a new
 * run; old directories simply stop being used and can be deleted by hand.
 */

#define CACHE_PATH_CAPACITY 1024u
#define CACHE_PIPELINE_VERSION 2u

typedef struct {
    char directory[CACHE_PATH_CAPACITY];
    bool ready;
} cache;

uint64_t cache_key(const char *input_path, uint64_t input_size, uint32_t page_count, uint32_t max_pixels, uint32_t pipeline_version);
bool cache_init(cache *value, const char *work_dir, uint64_t key, char *error, uint64_t error_capacity);
bool cache_page_path(const cache *value, uint32_t page_index, char *output, uint64_t output_capacity);
bool cache_read_page(const cache *value, uint32_t page_index, char *buffer, uint64_t capacity, uint64_t *length, bool *found);
bool cache_write_page(const cache *value, uint32_t page_index, const char *text, uint64_t length);
bool cache_write_manifest(const cache *value, const char *text, uint64_t length);

#endif