#ifndef BETTER_PDF_PNG_H
#define BETTER_PDF_PNG_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Minimal PNG writer.
 *
 * The raster is sent to llama-server over loopback, so compression buys
 * nothing: the encoder writes zlib "stored" deflate blocks, which any PNG
 * reader accepts and which keeps the encoder small enough to reason about.
 * Grayscale pages (the common case for scanned books) use one byte per pixel.
 */

typedef enum {
    PNG_MODE_GRAY = 0,
    PNG_MODE_RGB = 2
} png_mode;

uint64_t png_encoded_capacity(uint32_t width, uint32_t height, png_mode mode);
uint64_t png_encode(const uint8_t *pixels, uint32_t width, uint32_t height, png_mode mode, uint8_t *output, uint64_t output_capacity);

/* Exposed so the checksum implementation is covered by explicit vectors. */
uint32_t png_crc32(const uint8_t *data, uint64_t length);
uint32_t png_adler32(const uint8_t *data, uint64_t length);

#endif