#include "cache.h"

#include "asserts.h"
#include "platform_win32.h"

#include <stdio.h>
#include <string.h>

#define CACHE_FNV_OFFSET 0xcbf29ce484222325ull
#define CACHE_FNV_PRIME 0x100000001b3ull

static uint64_t cache_mix(uint64_t hash, const void *data, uint64_t length) {
    const uint8_t *bytes = (const uint8_t *)data;
    for (uint64_t index = 0; index < length; index += 1) {
        hash ^= (uint64_t)bytes[index];
        hash *= CACHE_FNV_PRIME;
    }
    return hash;
}

uint64_t cache_key(const char *input_path, uint64_t input_size, uint32_t page_count, uint32_t max_pixels, uint32_t pipeline_version) {
    ASSERT(input_path != NULL);
    uint64_t hash = CACHE_FNV_OFFSET;
    hash = cache_mix(hash, &pipeline_version, sizeof pipeline_version);
    hash = cache_mix(hash, &input_size, sizeof input_size);
    hash = cache_mix(hash, &page_count, sizeof page_count);
    hash = cache_mix(hash, &max_pixels, sizeof max_pixels);
    hash = cache_mix(hash, input_path, (uint64_t)strlen(input_path));
    return hash;
}

bool cache_init(cache *value, const char *work_dir, uint64_t key, char *error, uint64_t error_capacity) {
    ASSERT(value != NULL);
    ASSERT(work_dir != NULL);
    ASSERT(error != NULL);

    uint64_t written = (uint64_t)snprintf(value->directory, sizeof value->directory, "%s\\pages\\%016llx", work_dir, (unsigned long long)key);
    if (written >= (uint64_t)sizeof value->directory) {
        (void)snprintf(error, (size_t)error_capacity, "work directory path is too long");
        value->ready = false;
        return false;
    }
    if (!platform_create_directories(value->directory)) {
        (void)snprintf(error, (size_t)error_capacity, "could not create cache directory %s", value->directory);
        value->ready = false;
        return false;
    }
    value->ready = true;
    return true;
}

bool cache_page_path(const cache *value, uint32_t page_index, char *output, uint64_t output_capacity) {
    ASSERT(value != NULL);
    ASSERT(output != NULL);
    ASSERT(value->ready);

    uint64_t written = (uint64_t)snprintf(output, (size_t)output_capacity, "%s\\%05u.txt", value->directory, page_index + 1u);
    return written < output_capacity;
}

bool cache_read_page(const cache *value, uint32_t page_index, char *buffer, uint64_t capacity, uint64_t *length, bool *found) {
    ASSERT(value != NULL);
    ASSERT(buffer != NULL);
    ASSERT(length != NULL);
    ASSERT(found != NULL);

    char path[CACHE_PATH_CAPACITY];
    if (!cache_page_path(value, page_index, path, sizeof path)) {
        return false;
    }
    return platform_read_file(path, buffer, capacity, length, found);
}

bool cache_write_page(const cache *value, uint32_t page_index, const char *text, uint64_t length) {
    ASSERT(value != NULL);
    ASSERT(text != NULL);

    char path[CACHE_PATH_CAPACITY];
    if (!cache_page_path(value, page_index, path, sizeof path)) {
        return false;
    }
    return platform_write_file(path, text, length);
}

bool cache_write_manifest(const cache *value, const char *text, uint64_t length) {
    ASSERT(value != NULL);
    ASSERT(text != NULL);

    char path[CACHE_PATH_CAPACITY];
    uint64_t written = (uint64_t)snprintf(path, sizeof path, "%s\\manifest.txt", value->directory);
    if (written >= (uint64_t)sizeof path) {
        return false;
    }
    return platform_write_file(path, text, length);
}