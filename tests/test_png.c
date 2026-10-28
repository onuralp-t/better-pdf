#include "test.h"

#include "png.h"

#include <string.h>

/* Reference encodings produced by Python's zlib (level 0) and verified to
 * decode with Pillow. They pin the exact byte layout of every chunk. */
static const uint8_t gray_1x1_expected[70] = {
    0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au, 0x00u, 0x00u, 0x00u, 0x0Du,
    0x49u, 0x48u, 0x44u, 0x52u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u,
    0x08u, 0x00u, 0x00u, 0x00u, 0x00u, 0x3Au, 0x7Eu, 0x9Bu, 0x55u, 0x00u, 0x00u, 0x00u,
    0x0Du, 0x49u, 0x44u, 0x41u, 0x54u, 0x78u, 0x01u, 0x01u, 0x02u, 0x00u, 0xFDu, 0xFFu,
    0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x01u, 0x7Eu, 0x05u, 0x0Du, 0xD2u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x49u, 0x45u, 0x4Eu, 0x44u, 0xAEu, 0x42u, 0x60u, 0x82u,
};

static const uint8_t gray_2x2_expected[74] = {
    0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au, 0x00u, 0x00u, 0x00u, 0x0Du,
    0x49u, 0x48u, 0x44u, 0x52u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x02u,
    0x08u, 0x00u, 0x00u, 0x00u, 0x00u, 0x57u, 0xDDu, 0x52u, 0xF8u, 0x00u, 0x00u, 0x00u,
    0x11u, 0x49u, 0x44u, 0x41u, 0x54u, 0x78u, 0x01u, 0x01u, 0x06u, 0x00u, 0xF9u, 0xFFu,
    0x00u, 0x00u, 0xFFu, 0x00u, 0x80u, 0x40u, 0x05u, 0x42u, 0x01u, 0xC0u, 0x79u, 0xE9u,
    0x09u, 0xC4u, 0x00u, 0x00u, 0x00u, 0x00u, 0x49u, 0x45u, 0x4Eu, 0x44u, 0xAEu, 0x42u,
    0x60u, 0x82u,
};

static const uint8_t rgb_2x1_expected[75] = {
    0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au, 0x00u, 0x00u, 0x00u, 0x0Du,
    0x49u, 0x48u, 0x44u, 0x52u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x01u,
    0x08u, 0x02u, 0x00u, 0x00u, 0x00u, 0x7Bu, 0x40u, 0xE8u, 0xDDu, 0x00u, 0x00u, 0x00u,
    0x12u, 0x49u, 0x44u, 0x41u, 0x54u, 0x78u, 0x01u, 0x01u, 0x07u, 0x00u, 0xF8u, 0xFFu,
    0x00u, 0xFFu, 0x00u, 0x00u, 0x00u, 0xFFu, 0x00u, 0x07u, 0xFFu, 0x01u, 0xFFu, 0xC5u,
    0x0Eu, 0xE2u, 0x6Au, 0x00u, 0x00u, 0x00u, 0x00u, 0x49u, 0x45u, 0x4Eu, 0x44u, 0xAEu,
    0x42u, 0x60u, 0x82u,
};

static void check_bytes(const uint8_t *actual, uint64_t actual_length, const uint8_t *expected, uint64_t expected_length) {
    CHECK_U64_EQ(actual_length, expected_length);
    uint64_t compare = actual_length < expected_length ? actual_length : expected_length;
    for (uint64_t index = 0; index < compare; index += 1) {
        if (actual[index] != expected[index]) {
            char detail[128];
            (void)snprintf(detail, sizeof detail, "byte %llu: expected 0x%02X, actual 0x%02X",
                           (unsigned long long)index, expected[index], actual[index]);
            CHECK_DETAIL(false, detail);
            return;
        }
    }
    CHECK(true);
}

static void test_png_crc32_vector(void) {
    CHECK_U64_EQ(png_crc32((const uint8_t *)"123456789", 9), 0xCBF43926u);
    CHECK_U64_EQ(png_crc32((const uint8_t *)"", 0), 0u);
}

static void test_png_adler32_vector(void) {
    CHECK_U64_EQ(png_adler32((const uint8_t *)"", 0), 1u);
    CHECK_U64_EQ(png_adler32((const uint8_t *)"Wikipedia", 9), 0x11E60398u);
}

static void test_png_gray_1x1_byte_exact(void) {
    const uint8_t pixels[] = { 0x00u };
    uint8_t output[256];
    uint64_t written = png_encode(pixels, 1u, 1u, PNG_MODE_GRAY, output, sizeof output);
    check_bytes(output, written, gray_1x1_expected, sizeof gray_1x1_expected);
}

static void test_png_gray_2x2_byte_exact(void) {
    const uint8_t pixels[] = { 0x00u, 0xFFu, 0x80u, 0x40u };
    uint8_t output[256];
    uint64_t written = png_encode(pixels, 2u, 2u, PNG_MODE_GRAY, output, sizeof output);
    check_bytes(output, written, gray_2x2_expected, sizeof gray_2x2_expected);
}

static void test_png_rgb_2x1_byte_exact(void) {
    const uint8_t pixels[] = { 0xFFu, 0x00u, 0x00u, 0x00u, 0xFFu, 0x00u };
    uint8_t output[256];
    uint64_t written = png_encode(pixels, 2u, 1u, PNG_MODE_RGB, output, sizeof output);
    check_bytes(output, written, rgb_2x1_expected, sizeof rgb_2x1_expected);
}

static void test_png_odd_width_lengths(void) {
    uint8_t pixels[3u * 2u * 3u];
    memset(pixels, 0x33, sizeof pixels);
    uint8_t output[512];
    uint64_t gray = png_encode(pixels, 3u, 2u, PNG_MODE_GRAY, output, sizeof output);
    CHECK_U64_EQ(gray, 76u);
    CHECK_U64_EQ(png_encoded_capacity(3u, 2u, PNG_MODE_GRAY), 92u);

    uint64_t rgb = png_encode(pixels, 3u, 2u, PNG_MODE_RGB, output, sizeof output);
    CHECK_U64_EQ(rgb, 88u);
    CHECK_U64_EQ(png_encoded_capacity(3u, 2u, PNG_MODE_RGB), 104u);
}

static void test_png_capacity_boundary(void) {
    static uint8_t pixels[16u * 16u];
    memset(pixels, 0x7F, sizeof pixels);
    uint64_t capacity = png_encoded_capacity(16u, 16u, PNG_MODE_GRAY);
    static uint8_t output[4096];
    CHECK(capacity <= sizeof output);
    CHECK_U64_EQ(png_encode(pixels, 16u, 16u, PNG_MODE_GRAY, output, capacity - 1u), 0u);
    uint64_t written = png_encode(pixels, 16u, 16u, PNG_MODE_GRAY, output, capacity);
    CHECK(written > 0u);
    CHECK(written <= capacity);
}

static void test_png_multi_block(void) {
    enum { WIDTH = 256u, HEIGHT = 300u };
    static uint8_t pixels[WIDTH * HEIGHT];
    static uint8_t output[WIDTH * HEIGHT * 2u];
    for (uint64_t index = 0; index < sizeof pixels; index += 1) {
        pixels[index] = (uint8_t)(index * 7u);
    }

    uint64_t written = png_encode(pixels, WIDTH, HEIGHT, PNG_MODE_GRAY, output, sizeof output);
    uint64_t filtered = (uint64_t)HEIGHT * (WIDTH + 1u);
    uint64_t blocks = (filtered + 65534u) / 65535u;
    CHECK_U64_EQ(blocks, 2u);
    CHECK_U64_EQ(written, 57u + 2u + filtered + blocks * 5u + 4u);

    /* The IDAT payload starts after the 8-byte signature, the 25-byte IHDR and
     * 8 bytes of IDAT chunk framing. */
    const uint8_t *zlib = output + 8u + 25u + 8u;
    CHECK_U64_EQ(zlib[0], 0x78u);
    CHECK_U64_EQ(zlib[1], 0x01u);
    uint32_t first_length = (uint32_t)zlib[2u + 1u] | ((uint32_t)zlib[2u + 2u] << 8);
    CHECK_U64_EQ(first_length, 65535u);
    CHECK_U64_EQ(zlib[2], 0u);
    uint64_t second = 2u + 5u + 65535u;
    uint32_t second_length = (uint32_t)zlib[second + 1u] | ((uint32_t)zlib[second + 2u] << 8);
    CHECK_U64_EQ(second_length, filtered - 65535u);
    CHECK_U64_EQ(zlib[second], 1u);
}


static void test_png_large_image_spans_blocks(void) {
    static uint8_t pixels[200u * 300u * 3u];
    static uint8_t output[200u * 300u * 3u + 4096u];
    for (uint32_t index = 0; index < sizeof pixels; index += 1) {
        pixels[index] = (uint8_t)(index % 251u);
    }
    uint64_t capacity = png_encoded_capacity(200u, 300u, PNG_MODE_RGB);
    CHECK(capacity <= sizeof output);
    uint64_t length = png_encode(pixels, 200u, 300u, PNG_MODE_RGB, output, capacity);
    CHECK(length > 65535u);
    CHECK(length <= capacity);
    CHECK_U64_EQ(output[0], 0x89u);
    CHECK_U64_EQ(output[12], 'I');
    CHECK_U64_EQ(output[13], 'H');
    CHECK_U64_EQ(output[14], 'D');
    CHECK_U64_EQ(output[15], 'R');
    CHECK_U64_EQ(output[16], 0u);
    CHECK_U64_EQ(output[17], 0u);
    CHECK_U64_EQ(output[18], 0u);
    CHECK_U64_EQ(output[19], 200u);
    CHECK_U64_EQ(output[20], 0u);
    CHECK_U64_EQ(output[21], 0u);
    CHECK_U64_EQ(output[22], 1u);
    CHECK_U64_EQ(output[23], 44u);
    CHECK_U64_EQ(output[24], 8u);
    CHECK_U64_EQ(output[25], 2u);
}

void register_png_tests(void) {
    test_register("png_crc32_vector", test_png_crc32_vector);
    test_register("png_adler32_vector", test_png_adler32_vector);
    test_register("png_gray_1x1_byte_exact", test_png_gray_1x1_byte_exact);
    test_register("png_gray_2x2_byte_exact", test_png_gray_2x2_byte_exact);
    test_register("png_rgb_2x1_byte_exact", test_png_rgb_2x1_byte_exact);
    test_register("png_odd_width_lengths", test_png_odd_width_lengths);
    test_register("png_capacity_boundary", test_png_capacity_boundary);
    test_register("png_multi_block", test_png_multi_block);
    test_register("png_large_image_spans_blocks", test_png_large_image_spans_blocks);
}