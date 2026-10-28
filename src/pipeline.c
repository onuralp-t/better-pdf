#include "pipeline.h"

#include "asserts.h"
#include "base64.h"
#include "document.h"
#include "png.h"

#include <stdio.h>
#include <string.h>

#define PIPELINE_REQUEST_PRELUDE "{\"messages\":[{\"role\":\"user\",\"content\":[{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:image/png;base64,"
#define PIPELINE_REQUEST_SUFFIX "\"}},{\"type\":\"text\",\"text\":\"OCR:\"}]}],\"temperature\":0,\"max_tokens\":4096}"

uint64_t pipeline_text_capacity(uint32_t page_count) {
    return document_text_capacity(page_count);
}

bool pipeline_plan_sizes(uint32_t page_count, uint32_t max_pixels, pipeline_sizes *sizes) {
    ASSERT(sizes != NULL);
    if (page_count == 0u || max_pixels == 0u) {
        return false;
    }

    uint64_t text_capacity = document_text_capacity(page_count);
    sizes->document_bytes = document_memory_required(page_count, text_capacity);
    sizes->size_array_bytes = (uint64_t)page_count * 4u * 2u;
    sizes->raster_bytes = (uint64_t)max_pixels * 4u;
    sizes->png_bytes = (uint64_t)max_pixels * 4u + 4096u;
    sizes->base64_bytes = base64_encoded_length(sizes->png_bytes) + 8u;
    sizes->request_bytes = sizes->base64_bytes + 1024u;
    sizes->response_bytes = (uint64_t)PIPELINE_RESPONSE_BYTES;
    sizes->typst_bytes = text_capacity * 2u + (uint64_t)PIPELINE_TYPST_SLACK_BYTES;
    sizes->page_text_bytes = (uint64_t)PIPELINE_PAGE_TEXT_BYTES;

    sizes->total_bytes = sizes->document_bytes + sizes->size_array_bytes + sizes->raster_bytes +
                         sizes->png_bytes + sizes->base64_bytes + sizes->request_bytes +
                         sizes->response_bytes + sizes->typst_bytes + sizes->page_text_bytes;
    if (sizes->total_bytes > PIPELINE_MEMORY_LIMIT_BYTES) {
        return false;
    }
    return true;
}

bool pipeline_select_range(uint32_t page_count, uint32_t first_page, uint32_t last_page, uint32_t *first_index, uint32_t *last_index) {
    ASSERT(first_index != NULL);
    ASSERT(last_index != NULL);

    uint32_t last = last_page == 0u ? page_count : last_page;
    if (first_page == 0u || first_page > page_count || last > page_count || first_page > last) {
        return false;
    }
    *first_index = first_page - 1u;
    *last_index = last - 1u;
    return true;
}

bool pipeline_work_directory(const char *output_path, const char *work_dir_option, char *buffer, uint64_t capacity) {
    ASSERT(output_path != NULL);
    ASSERT(buffer != NULL);
    ASSERT(capacity > 0);

    if (work_dir_option != NULL) {
        uint64_t length = (uint64_t)strlen(work_dir_option);
        if (length + 1u > capacity) {
            return false;
        }
        memcpy(buffer, work_dir_option, (size_t)length + 1u);
        return true;
    }
    int written = snprintf(buffer, (size_t)capacity, "%s.work", output_path);
    return written > 0 && (uint64_t)written < capacity;
}

bool pipeline_file_stem(const char *path, char *buffer, uint64_t capacity) {
    ASSERT(path != NULL);
    ASSERT(buffer != NULL);
    ASSERT(capacity > 0);

    const char *name = path;
    for (const char *cursor = path; *cursor != '\0'; cursor += 1) {
        if (*cursor == '\\' || *cursor == '/') {
            name = cursor + 1;
        }
    }
    uint64_t length = (uint64_t)strlen(name);
    if (length > 4u && name[length - 4u] == '.' && (name[length - 3u] == 'p' || name[length - 3u] == 'P') &&
        (name[length - 2u] == 'd' || name[length - 2u] == 'D') && (name[length - 1u] == 'f' || name[length - 1u] == 'F')) {
        length -= 4u;
    }
    if (length == 0u) {
        const char *fallback = "document";
        length = (uint64_t)strlen(fallback);
        memcpy(buffer, fallback, (size_t)length);
        buffer[length] = '\0';
        return length + 1u <= capacity;
    }
    if (length + 1u > capacity) {
        return false;
    }
    memcpy(buffer, name, (size_t)length);
    buffer[length] = '\0';
    return true;
}

uint64_t pipeline_request_build(const char *base64_text, uint64_t base64_length, char *request, uint64_t capacity) {
    ASSERT(base64_text != NULL);
    ASSERT(request != NULL);

    uint64_t prelude_length = (uint64_t)strlen(PIPELINE_REQUEST_PRELUDE);
    uint64_t suffix_length = (uint64_t)strlen(PIPELINE_REQUEST_SUFFIX);
    uint64_t total = prelude_length + base64_length + suffix_length;
    if (total > capacity) {
        return 0;
    }
    memcpy(request, PIPELINE_REQUEST_PRELUDE, (size_t)prelude_length);
    if (base64_length > 0) {
        memcpy(request + prelude_length, base64_text, (size_t)base64_length);
    }
    memcpy(request + prelude_length + base64_length, PIPELINE_REQUEST_SUFFIX, (size_t)suffix_length);
    return total;
}