#include "test.h"

#include "geometry.h"

static void test_geometry_portrait(void) {
    const uint32_t widths[] = { 4308u, 4308u, 4308u };
    const uint32_t heights[] = { 6462u, 6462u, 6462u };
    page_geometry geometry;
    CHECK(geometry_from_page_sizes(widths, heights, 3u, 105u, &geometry));
    CHECK_U64_EQ(geometry.page_width_pt10, 4308u);
    CHECK_U64_EQ(geometry.page_height_pt10, 6462u);
    CHECK_U64_EQ(geometry.margin_x_pt10, 560u);
    CHECK_U64_EQ(geometry.margin_y_pt10, 646u);
    CHECK_U64_EQ(geometry.body_pt10, 105u);
}

static void test_geometry_clamps_margins(void) {
    const uint32_t big_width[] = { 10000u };
    const uint32_t big_height[] = { 15000u };
    page_geometry geometry;
    CHECK(geometry_from_page_sizes(big_width, big_height, 1u, 105u, &geometry));
    CHECK_U64_EQ(geometry.margin_x_pt10, 960u);
    CHECK_U64_EQ(geometry.margin_y_pt10, 720u);

    const uint32_t small_width[] = { 1000u };
    const uint32_t small_height[] = { 2000u };
    CHECK(geometry_from_page_sizes(small_width, small_height, 1u, 105u, &geometry));
    CHECK_U64_EQ(geometry.margin_x_pt10, 360u);
    CHECK_U64_EQ(geometry.margin_y_pt10, 300u);
}

static void test_geometry_swaps_landscape(void) {
    const uint32_t widths[] = { 7090u };
    const uint32_t heights[] = { 5550u };
    page_geometry geometry;
    CHECK(geometry_from_page_sizes(widths, heights, 1u, 105u, &geometry));
    CHECK_U64_EQ(geometry.page_width_pt10, 5550u);
    CHECK_U64_EQ(geometry.page_height_pt10, 7090u);
    CHECK_U64_EQ(geometry.margin_x_pt10, 721u);
    CHECK_U64_EQ(geometry.margin_y_pt10, 709u);
}

static void test_geometry_median_ignores_outliers(void) {
    const uint32_t widths[] = { 1000u, 9000u, 2000u, 2100u, 2050u };
    const uint32_t heights[] = { 2000u, 9000u, 3000u, 3100u, 3050u };
    page_geometry geometry;
    CHECK(geometry_from_page_sizes(widths, heights, 5u, 105u, &geometry));
    CHECK_U64_EQ(geometry.page_width_pt10, 2050u);
    CHECK_U64_EQ(geometry.page_height_pt10, 3050u);
}

static void test_geometry_samples_large_books(void) {
    static uint32_t widths[2000];
    static uint32_t heights[2000];
    for (uint32_t index = 0; index < 2000u; index += 1) {
        widths[index] = 4000u;
        heights[index] = 6000u;
    }
    page_geometry geometry;
    CHECK(geometry_from_page_sizes(widths, heights, 2000u, 105u, &geometry));
    CHECK_U64_EQ(geometry.page_width_pt10, 4000u);
    CHECK_U64_EQ(geometry.page_height_pt10, 6000u);
}

static void test_geometry_rejects_empty_and_zero(void) {
    page_geometry geometry;
    const uint32_t widths[] = { 0u };
    const uint32_t heights[] = { 6000u };
    CHECK(!geometry_from_page_sizes(widths, heights, 0u, 105u, &geometry));
    CHECK(!geometry_from_page_sizes(widths, heights, 1u, 105u, &geometry));
}


static void test_geometry_sorts_descending_and_zero_height(void) {
    const uint32_t widths[] = { 3000u, 2000u, 1000u };
    const uint32_t heights[] = { 5000u, 4000u, 3000u };
    page_geometry geometry;
    CHECK(geometry_from_page_sizes(widths, heights, 3u, 100u, &geometry));
    CHECK_U64_EQ(geometry.page_width_pt10, 2000u);
    CHECK_U64_EQ(geometry.page_height_pt10, 4000u);

    const uint32_t flat_width[] = { 4000u };
    const uint32_t flat_height[] = { 0u };
    CHECK(!geometry_from_page_sizes(flat_width, flat_height, 1u, 100u, &geometry));
}

void register_geometry_tests(void) {
    test_register("geometry_portrait", test_geometry_portrait);
    test_register("geometry_clamps_margins", test_geometry_clamps_margins);
    test_register("geometry_swaps_landscape", test_geometry_swaps_landscape);
    test_register("geometry_median_ignores_outliers", test_geometry_median_ignores_outliers);
    test_register("geometry_samples_large_books", test_geometry_samples_large_books);
    test_register("geometry_rejects_empty_and_zero", test_geometry_rejects_empty_and_zero);
    test_register("geometry_sorts_descending_and_zero_height", test_geometry_sorts_descending_and_zero_height);
}