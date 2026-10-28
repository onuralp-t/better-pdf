#ifndef BETTER_PDF_JSON_H
#define BETTER_PDF_JSON_H

#include <stdbool.h>
#include <stdint.h>

/*
 * A small, non-recursive JSON reader.
 *
 * The program only ever reads one shape of response from llama-server, so the
 * reader exposes a scanner that yields the span of a value plus lookups by key
 * and by array index. Container scanning is iterative: it counts delimiters
 * and skips strings, which is all the validation needed to walk well-formed
 * server responses without recursing or allocating.
 */

typedef enum {
    JSON_KIND_NULL = 0,
    JSON_KIND_BOOL = 1,
    JSON_KIND_NUMBER = 2,
    JSON_KIND_STRING = 3,
    JSON_KIND_OBJECT = 4,
    JSON_KIND_ARRAY = 5
} json_kind;

typedef struct {
    json_kind kind;
    uint64_t begin;
    uint64_t end;
} json_value;

bool json_parse_document(const char *text, uint64_t length, json_value *value);
bool json_scan_value(const char *text, uint64_t length, uint64_t offset, json_value *value);
bool json_object_get(const char *text, uint64_t length, json_value object, const char *key, json_value *value);
bool json_array_get(const char *text, uint64_t length, json_value array, uint64_t index, json_value *value);
bool json_string_copy(const char *text, uint64_t length, json_value value, char *output, uint64_t output_capacity, uint64_t *written);
bool json_number_to_u64(const char *text, uint64_t length, json_value value, uint64_t *number);

#endif