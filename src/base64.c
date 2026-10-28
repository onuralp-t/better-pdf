#include "base64.h"

#include "asserts.h"

static const char base64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

uint64_t base64_encoded_length(uint64_t input_length) {
    return ((input_length + 2u) / 3u) * 4u;
}

uint64_t base64_encode(const uint8_t *input, uint64_t input_length, char *output, uint64_t output_capacity) {
    ASSERT(input != NULL);
    ASSERT(output != NULL);

    uint64_t required = base64_encoded_length(input_length);
    if (required > output_capacity) {
        return 0;
    }

    uint64_t source_index = 0;
    uint64_t target_index = 0;
    while (source_index + 3u <= input_length) {
        uint32_t triple = ((uint32_t)input[source_index] << 16) |
                          ((uint32_t)input[source_index + 1u] << 8) |
                          (uint32_t)input[source_index + 2u];
        output[target_index] = base64_alphabet[(triple >> 18) & 0x3Fu];
        output[target_index + 1u] = base64_alphabet[(triple >> 12) & 0x3Fu];
        output[target_index + 2u] = base64_alphabet[(triple >> 6) & 0x3Fu];
        output[target_index + 3u] = base64_alphabet[triple & 0x3Fu];
        source_index += 3u;
        target_index += 4u;
    }

    uint64_t remaining = input_length - source_index;
    if (remaining == 1u) {
        uint32_t triple = (uint32_t)input[source_index] << 16;
        output[target_index] = base64_alphabet[(triple >> 18) & 0x3Fu];
        output[target_index + 1u] = base64_alphabet[(triple >> 12) & 0x3Fu];
        output[target_index + 2u] = '=';
        output[target_index + 3u] = '=';
    } else if (remaining == 2u) {
        uint32_t triple = ((uint32_t)input[source_index] << 16) | ((uint32_t)input[source_index + 1u] << 8);
        output[target_index] = base64_alphabet[(triple >> 18) & 0x3Fu];
        output[target_index + 1u] = base64_alphabet[(triple >> 12) & 0x3Fu];
        output[target_index + 2u] = base64_alphabet[(triple >> 6) & 0x3Fu];
        output[target_index + 3u] = '=';
    }

    return required;
}