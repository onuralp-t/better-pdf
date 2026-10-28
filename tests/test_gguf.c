#include "test.h"

#include "gguf.h"

#include <string.h>

#define BUILDER_CAPACITY 4096u

typedef struct {
    uint8_t data[BUILDER_CAPACITY];
    uint64_t length;
} builder;

static void put_bytes(builder *value, const void *data, uint64_t length) {
    CHECK(value->length + length <= BUILDER_CAPACITY);
    memcpy(value->data + value->length, data, (size_t)length);
    value->length += length;
}

static void put_u32(builder *value, uint32_t number) {
    uint8_t bytes[4];
    for (uint32_t index = 0; index < 4u; index += 1) {
        bytes[index] = (uint8_t)((number >> (index * 8u)) & 0xFFu);
    }
    put_bytes(value, bytes, 4u);
}

static void put_u64(builder *value, uint64_t number) {
    uint8_t bytes[8];
    for (uint32_t index = 0; index < 8u; index += 1) {
        bytes[index] = (uint8_t)((number >> (index * 8u)) & 0xFFu);
    }
    put_bytes(value, bytes, 8u);
}

static void put_string(builder *value, const char *text) {
    uint64_t length = (uint64_t)strlen(text);
    put_u64(value, length);
    put_bytes(value, text, length);
}

static void builder_header(builder *value, uint32_t version, uint64_t entry_count) {
    put_bytes(value, "GGUF", 4u);
    put_u32(value, version);
    put_u64(value, 0);
    put_u64(value, entry_count);
}

static void builder_key(builder *value, const char *key, uint32_t type) {
    put_string(value, key);
    put_u32(value, type);
}

static FILE *builder_file(builder *value, const char *name) {
    FILE *file = test_open_temporary(name);
    CHECK(file != NULL);
    if (file == NULL) {
        return NULL;
    }
    if (fwrite(value->data, 1, (size_t)value->length, file) != (size_t)value->length) {
        CHECK(false);
    }
    fflush(file);
    rewind(file);
    return file;
}

static void test_gguf_reads_vision_budget(void) {
    builder value = { { 0 }, 0 };
    value.length = 0;
    builder_header(&value, 3u, 3u);
    builder_key(&value, "clip.vision.image_min_pixels", 4u);
    put_u32(&value, 112896u);
    builder_key(&value, "clip.vision.image_max_pixels", 4u);
    put_u32(&value, 1003520u);
    builder_key(&value, "clip.vision.projector_type", 8u);
    put_string(&value, "paddleocr");

    FILE *file = builder_file(&value, "gguf_budget.gguf");
    uint64_t number = 0;
    bool found = false;
    CHECK(gguf_read_metadata_u64(file, "clip.vision.image_max_pixels", &number, &found));
    CHECK(found);
    CHECK_U64_EQ(number, 1003520u);
    rewind(file);
    CHECK(gguf_read_metadata_u64(file, "clip.vision.image_min_pixels", &number, &found));
    CHECK(found);
    CHECK_U64_EQ(number, 112896u);
    rewind(file);
    CHECK(!gguf_read_metadata_u64(file, "clip.vision.projector_type", &number, &found));
    fclose(file);
}

static void test_gguf_missing_key(void) {
    builder value = { { 0 }, 0 };
    value.length = 0;
    builder_header(&value, 2u, 1u);
    builder_key(&value, "some.key", 4u);
    put_u32(&value, 1u);

    FILE *file = builder_file(&value, "gguf_missing.gguf");
    uint64_t number = 0;
    bool found = true;
    CHECK(gguf_read_metadata_u64(file, "clip.vision.image_max_pixels", &number, &found));
    CHECK(!found);
    fclose(file);
}

static void test_gguf_scalar_types(void) {
    builder value = { { 0 }, 0 };
    value.length = 0;
    builder_header(&value, 3u, 10u);
    builder_key(&value, "u8", 0u);
    put_bytes(&value, "\xC8", 1u);
    builder_key(&value, "i8", 1u);
    put_bytes(&value, "\x05", 1u);
    builder_key(&value, "u16", 2u);
    put_bytes(&value, "\x60\xEA", 2u);
    builder_key(&value, "i16", 3u);
    put_bytes(&value, "\x2C\x01", 2u);
    builder_key(&value, "u32", 4u);
    put_u32(&value, 4000000000u);
    builder_key(&value, "i32", 5u);
    put_u32(&value, 7u);
    builder_key(&value, "bool", 7u);
    put_bytes(&value, "\x01", 1u);
    builder_key(&value, "u64", 10u);
    put_u64(&value, 1234567890123u);
    builder_key(&value, "i64", 11u);
    put_u64(&value, 42u);
    builder_key(&value, "target", 4u);
    put_u32(&value, 99u);

    struct {
        const char *key;
        uint64_t expected;
    } cases[] = {
        { "u8", 200u },
        { "i8", 5u },
        { "u16", 60000u },
        { "i16", 300u },
        { "u32", 4000000000u },
        { "i32", 7u },
        { "bool", 1u },
        { "u64", 1234567890123u },
        { "i64", 42u },
        { "target", 99u },
    };
    for (uint32_t index = 0; index < (uint32_t)(sizeof cases / sizeof cases[0]); index += 1) {
        FILE *file = builder_file(&value, "gguf_scalars.gguf");
        uint64_t number = 0;
        bool found = false;
        CHECK(gguf_read_metadata_u64(file, cases[index].key, &number, &found));
        CHECK(found);
        CHECK_U64_EQ(number, cases[index].expected);
        fclose(file);
    }
}

static void test_gguf_skips_strings_and_arrays(void) {
    builder value = { { 0 }, 0 };
    value.length = 0;
    builder_header(&value, 3u, 3u);
    builder_key(&value, "array.key", 9u);
    put_u32(&value, 4u);
    put_u64(&value, 3u);
    put_u32(&value, 1u);
    put_u32(&value, 2u);
    put_u32(&value, 3u);
    builder_key(&value, "string.key", 8u);
    put_string(&value, "a longer string value that has to be skipped");
    builder_key(&value, "clip.vision.image_max_pixels", 4u);
    put_u32(&value, 777u);

    FILE *file = builder_file(&value, "gguf_skip.gguf");
    uint64_t number = 0;
    bool found = false;
    CHECK(gguf_read_metadata_u64(file, "clip.vision.image_max_pixels", &number, &found));
    CHECK(found);
    CHECK_U64_EQ(number, 777u);
    fclose(file);
}

static void test_gguf_skips_array_of_strings(void) {
    builder value = { { 0 }, 0 };
    value.length = 0;
    builder_header(&value, 3u, 2u);
    builder_key(&value, "tokens", 9u);
    put_u32(&value, 8u);
    put_u64(&value, 2u);
    put_string(&value, "hello");
    put_string(&value, "world");
    builder_key(&value, "target", 4u);
    put_u32(&value, 5u);

    FILE *file = builder_file(&value, "gguf_string_array.gguf");
    uint64_t number = 0;
    bool found = false;
    CHECK(gguf_read_metadata_u64(file, "target", &number, &found));
    CHECK(found);
    CHECK_U64_EQ(number, 5u);
    fclose(file);
}

static void test_gguf_rejects_negative_and_float(void) {
    builder value = { { 0 }, 0 };
    value.length = 0;
    builder_header(&value, 3u, 5u);
    builder_key(&value, "negative32", 5u);
    put_u32(&value, 0xFFFFFFFFu);
    builder_key(&value, "negative8", 1u);
    put_bytes(&value, "\xFF", 1u);
    builder_key(&value, "negative16", 3u);
    put_bytes(&value, "\xFF\xFF", 2u);
    builder_key(&value, "negative64", 11u);
    put_u64(&value, UINT64_MAX);
    builder_key(&value, "float32", 6u);
    put_u32(&value, 0x3F800000u);

    FILE *file = builder_file(&value, "gguf_negative.gguf");
    uint64_t number = 0;
    bool found = false;
    CHECK(!gguf_read_metadata_u64(file, "negative32", &number, &found));
    rewind(file);
    CHECK(!gguf_read_metadata_u64(file, "negative8", &number, &found));
    rewind(file);
    CHECK(!gguf_read_metadata_u64(file, "negative16", &number, &found));
    rewind(file);
    CHECK(!gguf_read_metadata_u64(file, "negative64", &number, &found));
    rewind(file);
    CHECK(!gguf_read_metadata_u64(file, "float32", &number, &found));
    fclose(file);
}

static void test_gguf_rejects_bad_header(void) {
    builder value = { { 0 }, 0 };
    value.length = 0;
    put_bytes(&value, "NOPE", 4u);
    put_u32(&value, 3u);
    put_u64(&value, 0);
    put_u64(&value, 0);
    FILE *file = builder_file(&value, "gguf_bad_magic.gguf");
    uint64_t number = 0;
    bool found = false;
    CHECK(!gguf_read_metadata_u64(file, "k", &number, &found));
    fclose(file);

    builder old_version = { { 0 }, 0 };
    old_version.length = 0;
    builder_header(&old_version, 1u, 0u);
    file = builder_file(&old_version, "gguf_old_version.gguf");
    CHECK(!gguf_read_metadata_u64(file, "k", &number, &found));
    fclose(file);
}

static void test_gguf_rejects_truncated_file(void) {
    builder value = { { 0 }, 0 };
    value.length = 0;
    builder_header(&value, 3u, 1u);
    builder_key(&value, "u32", 4u);
    value.length -= 2u;

    FILE *file = builder_file(&value, "gguf_truncated.gguf");
    uint64_t number = 0;
    bool found = false;
    CHECK(!gguf_read_metadata_u64(file, "u32", &number, &found));
    fclose(file);
}

static void test_gguf_rejects_long_key(void) {
    builder value = { { 0 }, 0 };
    value.length = 0;
    builder_header(&value, 3u, 1u);
    static char long_key[600];
    memset(long_key, 'k', sizeof long_key - 1u);
    long_key[sizeof long_key - 1u] = '\0';
    put_string(&value, long_key);
    put_u32(&value, 4u);

    FILE *file = builder_file(&value, "gguf_long_key.gguf");
    uint64_t number = 0;
    bool found = false;
    CHECK(!gguf_read_metadata_u64(file, "u32", &number, &found));
    fclose(file);
}

static void test_gguf_rejects_unknown_type(void) {
    builder value = { { 0 }, 0 };
    value.length = 0;
    builder_header(&value, 3u, 2u);
    builder_key(&value, "unknown", 99u);
    builder_key(&value, "target", 4u);
    put_u32(&value, 5u);

    FILE *file = builder_file(&value, "gguf_unknown_type.gguf");
    uint64_t number = 0;
    bool found = false;
    CHECK(!gguf_read_metadata_u64(file, "target", &number, &found));
    fclose(file);
}

static void test_gguf_rejects_truncated_header(void) {
    uint64_t number = 0;
    bool found = true;

    /* Three bytes: not even the magic fits. */
    builder value = { { 0 }, 0 };
    put_bytes(&value, "GGU", 3u);
    FILE *file = builder_file(&value, "gguf_short_magic.gguf");
    CHECK(!gguf_read_metadata_u64(file, "k", &number, &found));
    CHECK(!found);
    fclose(file);

    /* Full magic and version, half of the tensor count. */
    value.length = 0;
    put_bytes(&value, "GGUF", 4u);
    put_u32(&value, 3u);
    put_u32(&value, 0u);
    file = builder_file(&value, "gguf_short_tensors.gguf");
    CHECK(!gguf_read_metadata_u64(file, "k", &number, &found));
    CHECK(!found);
    fclose(file);

    /* Full magic, half of the version. */
    value.length = 0;
    put_bytes(&value, "GGUF", 4u);
    put_bytes(&value, "\x03\x00", 2u);
    file = builder_file(&value, "gguf_short_version.gguf");
    CHECK(!gguf_read_metadata_u64(file, "k", &number, &found));
    CHECK(!found);
    fclose(file);

    /* Full tensor count, half of the entry count. */
    value.length = 0;
    put_bytes(&value, "GGUF", 4u);
    put_u32(&value, 3u);
    put_u64(&value, 0u);
    put_u32(&value, 0u);
    file = builder_file(&value, "gguf_short_entries.gguf");
    CHECK(!gguf_read_metadata_u64(file, "k", &number, &found));
    CHECK(!found);
    fclose(file);
}

static void test_gguf_rejects_future_version(void) {
    builder value = { { 0 }, 0 };
    builder_header(&value, 4u, 0u);

    FILE *file = builder_file(&value, "gguf_future_version.gguf");
    uint64_t number = 0;
    bool found = true;
    CHECK(!gguf_read_metadata_u64(file, "k", &number, &found));
    CHECK(!found);
    fclose(file);
}

static void test_gguf_rejects_unreadable_value(void) {
    uint64_t number = 0;
    bool found = true;

    /* A matching key whose value is a float. */
    builder value = { { 0 }, 0 };
    builder_header(&value, 3u, 1u);
    builder_key(&value, "k", 12u);
    put_u64(&value, 0x3FF0000000000000u);
    FILE *file = builder_file(&value, "gguf_float64.gguf");
    CHECK(!gguf_read_metadata_u64(file, "k", &number, &found));
    CHECK(!found);
    fclose(file);

    /* A matching key of a type the reader does not know. */
    value.length = 0;
    builder_header(&value, 3u, 1u);
    builder_key(&value, "k", 99u);
    file = builder_file(&value, "gguf_unknown_value.gguf");
    CHECK(!gguf_read_metadata_u64(file, "k", &number, &found));
    CHECK(!found);
    fclose(file);

    /* A matching key whose value is cut short. */
    value.length = 0;
    builder_header(&value, 3u, 1u);
    builder_key(&value, "k", 10u);
    put_u32(&value, 1u);
    file = builder_file(&value, "gguf_cut_value.gguf");
    CHECK(!gguf_read_metadata_u64(file, "k", &number, &found));
    CHECK(!found);
    fclose(file);
}

static void test_gguf_rejects_unseekable_strings(void) {
    uint64_t number = 0;
    bool found = false;

    /* A string value longer than off_t can address cannot be skipped. */
    builder value = { { 0 }, 0 };
    builder_header(&value, 3u, 1u);
    builder_key(&value, "k", 8u);
    put_u64(&value, 0x8000000000000000u);
    FILE *file = builder_file(&value, "gguf_huge_string.gguf");
    CHECK(!gguf_read_metadata_u64(file, "target", &number, &found));
    fclose(file);

    /* The same length inside an array of strings. */
    value.length = 0;
    builder_header(&value, 3u, 1u);
    builder_key(&value, "k", 9u);
    put_u32(&value, 8u);
    put_u64(&value, 1u);
    put_u64(&value, 0x8000000000000000u);
    file = builder_file(&value, "gguf_huge_string_array.gguf");
    CHECK(!gguf_read_metadata_u64(file, "target", &number, &found));
    fclose(file);
}

static void test_gguf_rejects_bad_arrays(void) {
    uint64_t number = 0;
    bool found = false;

    /* An array whose elements are themselves arrays. */
    builder value = { { 0 }, 0 };
    builder_header(&value, 3u, 1u);
    builder_key(&value, "k", 9u);
    put_u32(&value, 9u);
    put_u64(&value, 0u);
    FILE *file = builder_file(&value, "gguf_array_of_arrays.gguf");
    CHECK(!gguf_read_metadata_u64(file, "target", &number, &found));
    fclose(file);

    /* An array of a scalar type that does not exist. */
    value.length = 0;
    builder_header(&value, 3u, 1u);
    builder_key(&value, "k", 9u);
    put_u32(&value, 99u);
    put_u64(&value, 2u);
    file = builder_file(&value, "gguf_array_unknown.gguf");
    CHECK(!gguf_read_metadata_u64(file, "target", &number, &found));
    fclose(file);

    /* An element count that would overflow the byte size. */
    value.length = 0;
    builder_header(&value, 3u, 1u);
    builder_key(&value, "k", 9u);
    put_u32(&value, 4u);
    put_u64(&value, UINT64_MAX);
    file = builder_file(&value, "gguf_array_overflow.gguf");
    CHECK(!gguf_read_metadata_u64(file, "target", &number, &found));
    fclose(file);
}

static void test_gguf_rejects_truncated_entries(void) {
    uint64_t number = 0;
    bool found = false;

    /* No room at all for the key length. */
    builder value = { { 0 }, 0 };
    builder_header(&value, 3u, 1u);
    FILE *file = builder_file(&value, "gguf_no_key.gguf");
    CHECK(!gguf_read_metadata_u64(file, "target", &number, &found));
    fclose(file);

    /* A key length whose bytes are cut short. */
    value.length = 0;
    builder_header(&value, 3u, 1u);
    put_u64(&value, 4u);
    put_bytes(&value, "ab", 2u);
    file = builder_file(&value, "gguf_cut_key.gguf");
    CHECK(!gguf_read_metadata_u64(file, "target", &number, &found));
    fclose(file);

    /* A complete key whose type is cut short. */
    value.length = 0;
    builder_header(&value, 3u, 1u);
    put_string(&value, "key");
    put_bytes(&value, "\x04\x00", 2u);
    file = builder_file(&value, "gguf_cut_type.gguf");
    CHECK(!gguf_read_metadata_u64(file, "target", &number, &found));
    fclose(file);

    /* A string value whose length is cut short. */
    value.length = 0;
    builder_header(&value, 3u, 1u);
    builder_key(&value, "k", 8u);
    put_bytes(&value, "\x05\x00", 2u);
    file = builder_file(&value, "gguf_cut_string_length.gguf");
    CHECK(!gguf_read_metadata_u64(file, "target", &number, &found));
    fclose(file);

    /* A string array element whose length is cut short. */
    value.length = 0;
    builder_header(&value, 3u, 1u);
    builder_key(&value, "k", 9u);
    put_u32(&value, 8u);
    put_u64(&value, 1u);
    put_bytes(&value, "\x05\x00", 2u);
    file = builder_file(&value, "gguf_cut_array_length.gguf");
    CHECK(!gguf_read_metadata_u64(file, "target", &number, &found));
    fclose(file);

    /* An array whose element type is cut short. */
    value.length = 0;
    builder_header(&value, 3u, 1u);
    put_string(&value, "key");
    put_u32(&value, 9u);
    put_bytes(&value, "\x08\x00", 2u);
    file = builder_file(&value, "gguf_cut_element_type.gguf");
    CHECK(!gguf_read_metadata_u64(file, "target", &number, &found));
    fclose(file);

    /* An array whose element count is cut short. */
    value.length = 0;
    builder_header(&value, 3u, 1u);
    put_string(&value, "key");
    put_u32(&value, 9u);
    put_u32(&value, 4u);
    put_bytes(&value, "\x01\x00\x00", 3u);
    file = builder_file(&value, "gguf_cut_element_count.gguf");
    CHECK(!gguf_read_metadata_u64(file, "target", &number, &found));
    fclose(file);
}

static void test_gguf_skips_empty_string(void) {
    builder value = { { 0 }, 0 };
    builder_header(&value, 3u, 2u);
    builder_key(&value, "empty", 8u);
    put_u64(&value, 0u);
    builder_key(&value, "target", 4u);
    put_u32(&value, 5u);

    FILE *file = builder_file(&value, "gguf_empty_string.gguf");
    uint64_t number = 0;
    bool found = false;
    CHECK(gguf_read_metadata_u64(file, "target", &number, &found));
    CHECK(found);
    CHECK_U64_EQ(number, 5u);
    fclose(file);
}

static void test_gguf_skips_empty_key(void) {
    builder value = { { 0 }, 0 };
    builder_header(&value, 3u, 2u);
    put_string(&value, "");
    put_u32(&value, 4u);
    put_u32(&value, 7u);
    builder_key(&value, "target", 4u);
    put_u32(&value, 5u);

    FILE *file = builder_file(&value, "gguf_empty_key.gguf");
    uint64_t number = 0;
    bool found = false;
    CHECK(gguf_read_metadata_u64(file, "target", &number, &found));
    CHECK(found);
    CHECK_U64_EQ(number, 5u);
    fclose(file);
}

void register_gguf_tests(void) {
    test_register("gguf_reads_vision_budget", test_gguf_reads_vision_budget);
    test_register("gguf_missing_key", test_gguf_missing_key);
    test_register("gguf_scalar_types", test_gguf_scalar_types);
    test_register("gguf_skips_strings_and_arrays", test_gguf_skips_strings_and_arrays);
    test_register("gguf_skips_array_of_strings", test_gguf_skips_array_of_strings);
    test_register("gguf_rejects_negative_and_float", test_gguf_rejects_negative_and_float);
    test_register("gguf_rejects_bad_header", test_gguf_rejects_bad_header);
    test_register("gguf_rejects_truncated_file", test_gguf_rejects_truncated_file);
    test_register("gguf_rejects_long_key", test_gguf_rejects_long_key);
    test_register("gguf_rejects_unknown_type", test_gguf_rejects_unknown_type);
    test_register("gguf_rejects_truncated_header", test_gguf_rejects_truncated_header);
    test_register("gguf_rejects_future_version", test_gguf_rejects_future_version);
    test_register("gguf_rejects_unreadable_value", test_gguf_rejects_unreadable_value);
    test_register("gguf_rejects_unseekable_strings", test_gguf_rejects_unseekable_strings);
    test_register("gguf_rejects_bad_arrays", test_gguf_rejects_bad_arrays);
    test_register("gguf_rejects_truncated_entries", test_gguf_rejects_truncated_entries);
    test_register("gguf_skips_empty_string", test_gguf_skips_empty_string);
    test_register("gguf_skips_empty_key", test_gguf_skips_empty_key);
}