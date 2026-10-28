#ifndef BETTER_PDF_BASE64_H
#define BETTER_PDF_BASE64_H

#include <stdint.h>

uint64_t base64_encoded_length(uint64_t input_length);
uint64_t base64_encode(const uint8_t *input, uint64_t input_length, char *output, uint64_t output_capacity);

#endif