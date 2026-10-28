#include "image.h"

#include "asserts.h"

#include <math.h>

bool image_plan(uint32_t page_width_pt10, uint32_t page_height_pt10, uint32_t max_pixels, image_size *size) {
    ASSERT(size != NULL);

    if (page_width_pt10 == 0u || page_height_pt10 == 0u || max_pixels == 0u) {
        return false;
    }

    double width_points = (double)page_width_pt10 / 10.0;
    double height_points = (double)page_height_pt10 / 10.0;
    double scale = sqrt((double)max_pixels / (width_points * height_points));

    uint64_t width = (uint64_t)(width_points * scale);
    uint64_t height = (uint64_t)(height_points * scale);
    if (width == 0u) {
        width = 1u;
    }
    if (height == 0u) {
        height = 1u;
    }
    while (width * height > (uint64_t)max_pixels) {
        if (width >= height) {
            width -= 1u;
        } else {
            height -= 1u;
        }
    }

    size->width = (uint32_t)width;
    size->height = (uint32_t)height;
    return true;
}

static uint32_t image_luma(uint8_t red, uint8_t green, uint8_t blue) {
    uint32_t value = 299u * (uint32_t)red + 587u * (uint32_t)green + 114u * (uint32_t)blue;
    return value / 1000u;
}

static uint8_t image_maximum(uint8_t first, uint8_t second, uint8_t third) {
    uint8_t result = first;
    if (second > result) {
        result = second;
    }
    if (third > result) {
        result = third;
    }
    return result;
}

static uint8_t image_minimum(uint8_t first, uint8_t second, uint8_t third) {
    uint8_t result = first;
    if (second < result) {
        result = second;
    }
    if (third < result) {
        result = third;
    }
    return result;
}

void image_analyze(const uint8_t *bgra, uint32_t width, uint32_t height, uint64_t stride, image_analysis *analysis) {
    ASSERT(bgra != NULL);
    ASSERT(analysis != NULL);
    ASSERT(width > 0);
    ASSERT(height > 0);
    ASSERT(stride >= (uint64_t)width * 4u);

    uint64_t ink_pixels = 0;
    uint32_t max_chroma = 0;
    for (uint32_t row = 0; row < height; row += 1) {
        const uint8_t *pixel = bgra + (uint64_t)row * stride;
        for (uint32_t column = 0; column < width; column += 1) {
            uint8_t blue = pixel[0];
            uint8_t green = pixel[1];
            uint8_t red = pixel[2];
            uint32_t chroma = (uint32_t)image_maximum(red, green, blue) - (uint32_t)image_minimum(red, green, blue);
            if (chroma > max_chroma) {
                max_chroma = chroma;
            }
            if (image_luma(red, green, blue) < IMAGE_INK_LUMA_THRESHOLD) {
                ink_pixels += 1u;
            }
            pixel += 4u;
        }
    }

    uint64_t pixel_count = (uint64_t)width * (uint64_t)height;
    uint64_t ink_percent = (ink_pixels * 1000u) / pixel_count;

    analysis->max_chroma = max_chroma;
    analysis->pixel_count = pixel_count;
    analysis->ink_pixels = ink_pixels;
    analysis->mode = max_chroma <= IMAGE_CHROMA_GRAY_LIMIT ? IMAGE_MODE_GRAY : IMAGE_MODE_RGB;
    if (ink_percent < IMAGE_BLANK_INK_PER_MILLE) {
        analysis->kind = PAGE_KIND_BLANK;
    } else if (ink_percent > IMAGE_DARK_INK_PER_MILLE) {
        analysis->kind = PAGE_KIND_DARK;
    } else {
        analysis->kind = PAGE_KIND_NORMAL;
    }
}

void image_convert_in_place(uint8_t *bgra, uint32_t width, uint32_t height, image_mode mode) {
    ASSERT(bgra != NULL);
    ASSERT(width > 0);
    ASSERT(height > 0);

    uint64_t pixel_count = (uint64_t)width * (uint64_t)height;
    for (uint64_t index = 0; index < pixel_count; index += 1) {
        uint8_t blue = bgra[index * 4u];
        uint8_t green = bgra[index * 4u + 1u];
        uint8_t red = bgra[index * 4u + 2u];
        if (mode == IMAGE_MODE_GRAY) {
            bgra[index] = (uint8_t)image_luma(red, green, blue);
        } else {
            bgra[index * 3u] = red;
            bgra[index * 3u + 1u] = green;
            bgra[index * 3u + 2u] = blue;
        }
    }
}