#ifndef BETTER_PDF_PIPELINE_H
#define BETTER_PDF_PIPELINE_H

#include "cli.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * Pipeline planning: the arithmetic and string work that decides how much
 * memory to allocate, which pages to process, where the work directory lives,
 * and what the OCR request looks like. Keeping it separate from the
 * orchestration makes all of it directly testable.
 */

#define PIPELINE_RESPONSE_BYTES (2u * 1024u * 1024u)
#define PIPELINE_TYPST_SLACK_BYTES (1024u * 1024u)
#define PIPELINE_PAGE_TEXT_BYTES (256u * 1024u)
#define PIPELINE_MEMORY_LIMIT_BYTES (2ull * 1024ull * 1024ull * 1024ull)

typedef struct {
    uint64_t document_bytes;
    uint64_t size_array_bytes;
    uint64_t raster_bytes;
    uint64_t png_bytes;
    uint64_t base64_bytes;
    uint64_t request_bytes;
    uint64_t response_bytes;
    uint64_t typst_bytes;
    uint64_t page_text_bytes;
    uint64_t total_bytes;
} pipeline_sizes;

typedef struct {
    uint32_t *page_widths;
    uint32_t *page_heights;
    uint8_t *raster;
    uint64_t raster_capacity;
    uint8_t *png;
    uint64_t png_capacity;
    char *base64;
    uint64_t base64_capacity;
    char *request;
    uint64_t request_capacity;
    char *response;
    uint64_t response_capacity;
    char *typst;
    uint64_t typst_capacity;
    char *page_text;
    uint64_t page_text_capacity;
} pipeline_buffers;

bool pipeline_plan_sizes(uint32_t page_count, uint32_t max_pixels, pipeline_sizes *sizes);
bool pipeline_select_range(uint32_t page_count, uint32_t first_page, uint32_t last_page, uint32_t *first_index, uint32_t *last_index);
bool pipeline_work_directory(const char *output_path, const char *work_dir_option, char *buffer, uint64_t capacity);
bool pipeline_file_stem(const char *path, char *buffer, uint64_t capacity);
uint64_t pipeline_request_build(const char *base64_text, uint64_t base64_length, char *request, uint64_t capacity);

#endif