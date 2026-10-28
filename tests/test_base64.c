#include "test.h"

#include "base64.h"

#include <string.h>

static void check_encoding(const uint8_t *input, uint64_t length, const char *expected) {
    char output[64];
    uint64_t written = base64_encode(input, length, output, sizeof output);
    CHECK_U64_EQ(written, strlen(expected));
    output[written] = '\0';
    CHECK_STR_EQ(output, expected);
    CHECK_U64_EQ(base64_encoded_length(length), strlen(expected));
}

static void test_base64_vectors(void) {
    check_encoding((const uint8_t *)"", 0, "");
    check_encoding((const uint8_t *)"f", 1, "Zg==");
    check_encoding((const uint8_t *)"fo", 2, "Zm8=");
    check_encoding((const uint8_t *)"foo", 3, "Zm9v");
    check_encoding((const uint8_t *)"foob", 4, "Zm9vYg==");
    check_encoding((const uint8_t *)"fooba", 5, "Zm9vYmE=");
    check_encoding((const uint8_t *)"foobar", 6, "Zm9vYmFy");
}

static void test_base64_binary_bytes(void) {
    const uint8_t input[] = { 0x00u, 0xFFu, 0x10u, 0x80u, 0x7Fu };
    char output[16];
    uint64_t written = base64_encode(input, sizeof input, output, sizeof output);
    CHECK_U64_EQ(written, 8u);
    output[written] = '\0';
    CHECK_STR_EQ(output, "AP8QgH8=");
}

static void test_base64_capacity_failure(void) {
    const uint8_t input[] = { 0x01u, 0x02u, 0x03u, 0x04u };
    char output[8];
    CHECK_U64_EQ(base64_encode(input, sizeof input, output, 7u), 0u);
    CHECK_U64_EQ(base64_encode(input, sizeof input, output, 8u), 8u);
}

static void test_base64_length_formula(void) {
    CHECK_U64_EQ(base64_encoded_length(0), 0);
    CHECK_U64_EQ(base64_encoded_length(1), 4);
    CHECK_U64_EQ(base64_encoded_length(2), 4);
    CHECK_U64_EQ(base64_encoded_length(3), 4);
    CHECK_U64_EQ(base64_encoded_length(4), 8);
    CHECK_U64_EQ(base64_encoded_length(3000000u), 4000000u);
}

void register_base64_tests(void) {
    test_register("base64_vectors", test_base64_vectors);
    test_register("base64_binary_bytes", test_base64_binary_bytes);
    test_register("base64_capacity_failure", test_base64_capacity_failure);
    test_register("base64_length_formula", test_base64_length_formula);
}