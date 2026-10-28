#include "geometry.h"

#include "asserts.h"

#include <string.h>

#define GEOMETRY_SAMPLE_CAPACITY 1024u
#define GEOMETRY_MARGIN_X_PERCENT 13u
#define GEOMETRY_MARGIN_Y_PERCENT 10u
#define GEOMETRY_MARGIN_X_MIN_PT10 360u
#define GEOMETRY_MARGIN_X_MAX_PT10 960u
#define GEOMETRY_MARGIN_Y_MIN_PT10 300u
#define GEOMETRY_MARGIN_Y_MAX_PT10 720u

static uint32_t geometry_clamp(uint32_t value, uint32_t minimum, uint32_t maximum) {
    ASSERT(minimum <= maximum);
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static void geometry_sort(uint32_t *values, uint32_t count) {
    ASSERT(values != NULL);
    ASSERT(count > 0u);
    for (uint32_t index = 1; index < count; index += 1) {
        uint32_t value = values[index];
        uint32_t position = index;
        while (position > 0u && values[position - 1u] > value) {
            values[position] = values[position - 1u];
            position -= 1u;
        }
        values[position] = value;
    }
}

static uint32_t geometry_median(uint32_t *values, uint32_t count) {
    ASSERT(values != NULL);
    ASSERT(count > 0);
    geometry_sort(values, count);
    return values[count / 2u];
}

bool geometry_from_page_sizes(const uint32_t *widths_pt10, const uint32_t *heights_pt10, uint32_t count, uint32_t body_pt10, page_geometry *geometry) {
    ASSERT(widths_pt10 != NULL);
    ASSERT(heights_pt10 != NULL);
    ASSERT(geometry != NULL);

    if (count == 0u) {
        return false;
    }
    uint32_t sample = count < GEOMETRY_SAMPLE_CAPACITY ? count : GEOMETRY_SAMPLE_CAPACITY;
    uint32_t widths[GEOMETRY_SAMPLE_CAPACITY];
    uint32_t heights[GEOMETRY_SAMPLE_CAPACITY];
    if (count <= GEOMETRY_SAMPLE_CAPACITY) {
        for (uint32_t index = 0; index < count; index += 1) {
            widths[index] = widths_pt10[index];
            heights[index] = heights_pt10[index];
        }
    } else {
        for (uint32_t index = 0; index < sample; index += 1) {
            uint32_t source = (uint32_t)(((uint64_t)index * (uint64_t)count) / (uint64_t)sample);
            widths[index] = widths_pt10[source];
            heights[index] = heights_pt10[source];
        }
    }

    uint32_t width = geometry_median(widths, sample);
    uint32_t height = geometry_median(heights, sample);
    if (width == 0u || height == 0u) {
        return false;
    }
    if (width > height) {
        uint32_t swap = width;
        width = height;
        height = swap;
    }

    geometry->page_width_pt10 = width;
    geometry->page_height_pt10 = height;
    geometry->margin_x_pt10 = geometry_clamp((width * GEOMETRY_MARGIN_X_PERCENT) / 100u, GEOMETRY_MARGIN_X_MIN_PT10, GEOMETRY_MARGIN_X_MAX_PT10);
    geometry->margin_y_pt10 = geometry_clamp((height * GEOMETRY_MARGIN_Y_PERCENT) / 100u, GEOMETRY_MARGIN_Y_MIN_PT10, GEOMETRY_MARGIN_Y_MAX_PT10);
    geometry->body_pt10 = body_pt10;
    return true;
}