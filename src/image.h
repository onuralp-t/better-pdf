#ifndef BETTER_PDF_IMAGE_H
#define BETTER_PDF_IMAGE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    IMAGE_MODE_GRAY = 0,
    IMAGE_MODE_RGB = 1
} image_mode;

typedef enum {
    PAGE_KIND_NORMAL = 0,
    PAGE_KIND_BLANK = 1,
    PAGE_KIND_DARK = 2
} page_kind;

typedef struct {
    uint32_t width;
    uint32_t height;
} image_size;

typedef struct {
    image_mode mode;
    page_kind kind;
    uint32_t max_chroma;
    uint64_t pixel_count;
    uint64_t ink_pixels;
} image_analysis;

#define IMAGE_CHROMA_GRAY_LIMIT 8u
#define IMAGE_INK_LUMA_THRESHOLD 160u
#define IMAGE_BLANK_INK_PER_MILLE 2u      /* below 0.2% ink the page is blank */
#define IMAGE_DARK_INK_PER_MILLE 850u     /* above 85% ink the page is a dark plate */

bool image_plan(uint32_t page_width_pt10, uint32_t page_height_pt10, uint32_t max_pixels, image_size *size);
void image_analyze(const uint8_t *bgra, uint32_t width, uint32_t height, uint64_t stride, image_analysis *analysis);

/* Converts the BGRA raster to gray or tightly packed RGB inside the same
 * buffer. Forward iteration is safe because the destination pixel always lies
 * at or before the source pixel. */
void image_convert_in_place(uint8_t *bgra, uint32_t width, uint32_t height, image_mode mode);

#endif