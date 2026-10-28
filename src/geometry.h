#ifndef BETTER_PDF_GEOMETRY_H
#define BETTER_PDF_GEOMETRY_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Output page geometry, derived from the input pages.
 *
 * A book's pages are nearly all the same size, so the median is a robust
 * stand-in for "the page". Spreads scanned sideways are normalized to portrait:
 * their text is still read as one column, and a portrait output keeps the line
 * length close to a real book page.
 */

typedef struct {
    uint32_t page_width_pt10;
    uint32_t page_height_pt10;
    uint32_t margin_x_pt10;
    uint32_t margin_y_pt10;
    uint32_t body_pt10;
} page_geometry;

bool geometry_from_page_sizes(const uint32_t *widths_pt10, const uint32_t *heights_pt10, uint32_t count, uint32_t body_pt10, page_geometry *geometry);

#endif