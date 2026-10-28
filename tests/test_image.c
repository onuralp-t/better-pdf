#include "test.h"

#include "image.h"

#include <string.h>

static void test_image_plan_portrait_page(void) {
    image_size size;
    CHECK(image_plan(4308u, 6462u, 1003520u, &size));
    CHECK_U64_EQ(size.width, 817u);
    CHECK_U64_EQ(size.height, 1226u);
    CHECK((uint64_t)size.width * size.height <= 1003520u);
}

static void test_image_plan_landscape_spread(void) {
    image_size size;
    CHECK(image_plan(7090u, 5550u, 1003520u, &size));
    CHECK((uint64_t)size.width * size.height <= 1003520u);
    CHECK(size.width > size.height);
    CHECK_U64_EQ(size.width, 1132u);
    CHECK_U64_EQ(size.height, 886u);
}

static void test_image_plan_rejects_invalid(void) {
    image_size size;
    CHECK(!image_plan(0u, 100u, 100000u, &size));
    CHECK(!image_plan(100u, 0u, 100000u, &size));
    CHECK(!image_plan(100u, 100u, 0u, &size));
}

static void test_image_plan_tiny_budget(void) {
    image_size size;
    CHECK(image_plan(10000u, 20000u, 16384u, &size));
    CHECK((uint64_t)size.width * size.height <= 16384u);
    CHECK_U64_EQ(size.width, 90u);
    CHECK_U64_EQ(size.height, 181u);
}

static void test_image_plan_extreme_aspect(void) {
    image_size size;
    CHECK(image_plan(100u, 20000u, 1003520u, &size));
    CHECK((uint64_t)size.width * size.height <= 1003520u);
    CHECK(size.width >= 1u);
    CHECK(size.height >= 1u);
    CHECK(size.height > size.width);
}

static void test_image_analyze_monochrome(void) {
    /* Two pixels: white then black, BGRA. */
    const uint8_t pixels[] = { 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0x00u, 0x00u, 0x00u, 0xFFu };
    image_analysis analysis;
    image_analyze(pixels, 2u, 1u, 8u, &analysis);
    CHECK_U64_EQ(analysis.ink_pixels, 1u);
    CHECK_U64_EQ(analysis.pixel_count, 2u);
    CHECK_U64_EQ(analysis.max_chroma, 0u);
    CHECK(analysis.mode == IMAGE_MODE_GRAY);
    CHECK(analysis.kind == PAGE_KIND_NORMAL);
}

static void test_image_analyze_blank_and_dark(void) {
    uint8_t white[4u * 4u * 4u];
    memset(white, 0xFF, sizeof white);
    image_analysis analysis;
    image_analyze(white, 4u, 4u, 16u, &analysis);
    CHECK(analysis.kind == PAGE_KIND_BLANK);
    CHECK_U64_EQ(analysis.ink_pixels, 0u);

    uint8_t dark[4u * 4u * 4u];
    for (uint64_t index = 0; index < sizeof dark; index += 4u) {
        dark[index] = 0u;
        dark[index + 1u] = 0u;
        dark[index + 2u] = 0u;
        dark[index + 3u] = 0xFFu;
    }
    image_analyze(dark, 4u, 4u, 16u, &analysis);
    CHECK(analysis.kind == PAGE_KIND_DARK);
    CHECK_U64_EQ(analysis.ink_pixels, 16u);
}

static void test_image_analyze_colour(void) {
    const uint8_t red[] = { 0x00u, 0x00u, 0xFFu, 0xFFu };
    image_analysis analysis;
    image_analyze(red, 1u, 1u, 4u, &analysis);
    CHECK_U64_EQ(analysis.max_chroma, 255u);
    CHECK(analysis.mode == IMAGE_MODE_RGB);
}

static void test_image_analyze_chroma_boundary(void) {
    const uint8_t gray_limit[] = { 0x00u, 0x00u, 0x08u, 0xFFu };
    const uint8_t colour[] = { 0x00u, 0x00u, 0x09u, 0xFFu };
    image_analysis analysis;
    image_analyze(gray_limit, 1u, 1u, 4u, &analysis);
    CHECK(analysis.mode == IMAGE_MODE_GRAY);
    image_analyze(colour, 1u, 1u, 4u, &analysis);
    CHECK(analysis.mode == IMAGE_MODE_RGB);
}

static void test_image_convert_gray_in_place(void) {
    uint8_t pixels[] = {
        0xFFu, 0xFFu, 0xFFu, 0xFFu,
        0x00u, 0x00u, 0x00u, 0xFFu,
        0x80u, 0x80u, 0x80u, 0xFFu,
    };
    image_convert_in_place(pixels, 3u, 1u, IMAGE_MODE_GRAY);
    CHECK_U64_EQ(pixels[0], 255u);
    CHECK_U64_EQ(pixels[1], 0u);
    CHECK_U64_EQ(pixels[2], 128u);
}

static void test_image_convert_rgb_in_place(void) {
    uint8_t pixels[] = {
        0x00u, 0x00u, 0xFFu, 0xFFu,
        0x00u, 0xFFu, 0x00u, 0xFFu,
        0xFFu, 0x00u, 0x00u, 0xFFu,
        0x11u, 0x22u, 0x33u, 0xFFu,
    };
    image_convert_in_place(pixels, 4u, 1u, IMAGE_MODE_RGB);
    CHECK_U64_EQ(pixels[0], 0xFFu);
    CHECK_U64_EQ(pixels[1], 0x00u);
    CHECK_U64_EQ(pixels[2], 0x00u);
    CHECK_U64_EQ(pixels[3], 0x00u);
    CHECK_U64_EQ(pixels[4], 0xFFu);
    CHECK_U64_EQ(pixels[5], 0x00u);
    CHECK_U64_EQ(pixels[6], 0x00u);
    CHECK_U64_EQ(pixels[7], 0x00u);
    CHECK_U64_EQ(pixels[8], 0xFFu);
    CHECK_U64_EQ(pixels[9], 0x33u);
    CHECK_U64_EQ(pixels[10], 0x22u);
    CHECK_U64_EQ(pixels[11], 0x11u);
}


static void test_image_plan_clamps_to_one_pixel(void) {
    image_size size;
    CHECK(image_plan(1u, 1000u, 100u, &size));
    CHECK_U64_EQ(size.width, 1u);
    CHECK_U64_EQ(size.height, 100u);
    CHECK(image_plan(1000u, 1u, 100u, &size));
    CHECK_U64_EQ(size.width, 100u);
    CHECK_U64_EQ(size.height, 1u);
    CHECK(image_plan(1000u, 1000u, 600u, &size));
    CHECK_U64_EQ(size.width, 24u);
    CHECK_U64_EQ(size.height, 24u);
}

static void test_image_analyze_secondary_channels(void) {
    const uint8_t pixels[] = {
        0x01u, 0xFAu, 0x05u, 0xFFu,
        0xFAu, 0xFAu, 0xFAu, 0xFFu,
    };
    image_analysis analysis;
    image_analyze(pixels, 2u, 1u, 8u, &analysis);
    CHECK_U64_EQ(analysis.max_chroma, 249u);
    CHECK_U64_EQ(analysis.ink_pixels, 1u);
    CHECK(analysis.mode == IMAGE_MODE_RGB);
}

static void test_image_analyze_blue_dominant(void) {
    /* Blue carries the largest channel here, so the third maximum wins. */
    const uint8_t blue[] = { 0xFFu, 0x00u, 0x00u, 0xFFu };
    image_analysis analysis;
    image_analyze(blue, 1u, 1u, 4u, &analysis);
    CHECK_U64_EQ(analysis.max_chroma, 255u);
    CHECK(analysis.mode == IMAGE_MODE_RGB);
    CHECK_U64_EQ(analysis.ink_pixels, 1u);
}

void register_image_tests(void) {
    test_register("image_plan_portrait_page", test_image_plan_portrait_page);
    test_register("image_plan_landscape_spread", test_image_plan_landscape_spread);
    test_register("image_plan_rejects_invalid", test_image_plan_rejects_invalid);
    test_register("image_plan_tiny_budget", test_image_plan_tiny_budget);
    test_register("image_plan_extreme_aspect", test_image_plan_extreme_aspect);
    test_register("image_analyze_monochrome", test_image_analyze_monochrome);
    test_register("image_analyze_blank_and_dark", test_image_analyze_blank_and_dark);
    test_register("image_analyze_colour", test_image_analyze_colour);
    test_register("image_analyze_chroma_boundary", test_image_analyze_chroma_boundary);
    test_register("image_convert_gray_in_place", test_image_convert_gray_in_place);
    test_register("image_convert_rgb_in_place", test_image_convert_rgb_in_place);
    test_register("image_plan_clamps_to_one_pixel", test_image_plan_clamps_to_one_pixel);
    test_register("image_analyze_secondary_channels", test_image_analyze_secondary_channels);
    test_register("image_analyze_blue_dominant", test_image_analyze_blue_dominant);
}