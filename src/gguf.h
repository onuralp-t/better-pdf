#ifndef BETTER_PDF_GGUF_H
#define BETTER_PDF_GGUF_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Reads a single numeric metadata value out of a GGUF file.
 *
 * The projector file is hundreds of megabytes; only its metadata header is
 * interesting, so this walks the key/value block from an open stream and skips
 * tensor data without ever mapping the file. The vision encoder caps the image
 * at clip.vision.image_max_pixels, and that number decides the render size.
 */

bool gguf_read_metadata_u64(FILE *file, const char *key, uint64_t *value, bool *found);

#endif