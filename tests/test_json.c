#include "test.h"

#include "json.h"

#include <string.h>

static uint64_t text_length(const char *text) {
    return (uint64_t)strlen(text);
}

static bool parse_ok(const char *text) {
    json_value value;
    return json_parse_document(text, text_length(text), &value);
}

static void test_json_document_shape(void) {
    const char *document = "{\"a\": 1, \"b\": \"x\", \"c\": true, \"d\": null, \"e\": [1, 2], \"f\": {\"g\": \"h\"}}";
    json_value root;
    CHECK(json_parse_document(document, text_length(document), &root));
    CHECK(root.kind == JSON_KIND_OBJECT);

    json_value value;
    CHECK(json_object_get(document, text_length(document), root, "a", &value));
    CHECK(value.kind == JSON_KIND_NUMBER);
    uint64_t number = 0;
    CHECK(json_number_to_u64(document, text_length(document), value, &number));
    CHECK_U64_EQ(number, 1);

    CHECK(json_object_get(document, text_length(document), root, "b", &value));
    CHECK(value.kind == JSON_KIND_STRING);
    char string[16];
    uint64_t written = 0;
    CHECK(json_string_copy(document, text_length(document), value, string, sizeof string, &written));
    CHECK_U64_EQ(written, 1u);
    string[written] = '\0';
    CHECK_STR_EQ(string, "x");

    CHECK(json_object_get(document, text_length(document), root, "c", &value));
    CHECK(value.kind == JSON_KIND_BOOL);
    CHECK(json_object_get(document, text_length(document), root, "d", &value));
    CHECK(value.kind == JSON_KIND_NULL);

    CHECK(json_object_get(document, text_length(document), root, "e", &value));
    CHECK(value.kind == JSON_KIND_ARRAY);
    json_value element;
    CHECK(json_array_get(document, text_length(document), value, 1u, &element));
    CHECK(json_number_to_u64(document, text_length(document), element, &number));
    CHECK_U64_EQ(number, 2);

    CHECK(json_object_get(document, text_length(document), root, "f", &value));
    CHECK(value.kind == JSON_KIND_OBJECT);
    CHECK(json_object_get(document, text_length(document), value, "g", &element));
    CHECK(json_string_copy(document, text_length(document), element, string, sizeof string, &written));
    string[written] = '\0';
    CHECK_STR_EQ(string, "h");
}

static void test_json_document_rejects_trailing_garbage(void) {
    CHECK(!parse_ok("{\"a\":1} x"));
    CHECK(parse_ok("{\"a\":1}"));
    CHECK(parse_ok("  \t\r\n{\"a\":1}  \n"));
    CHECK(!parse_ok(""));
}

static void test_json_missing_members_and_elements(void) {
    const char *document = "{}";
    json_value root;
    CHECK(json_parse_document(document, text_length(document), &root));
    json_value value;
    CHECK(!json_object_get(document, text_length(document), root, "a", &value));

    const char *array_document = "[]";
    CHECK(json_parse_document(array_document, text_length(array_document), &root));
    CHECK(!json_array_get(array_document, text_length(array_document), root, 0u, &value));

    const char *spaced = "{ \"a\" : 1 , \"b\" : 2 }";
    CHECK(json_parse_document(spaced, text_length(spaced), &root));
    CHECK(!json_object_get(spaced, text_length(spaced), root, "z", &value));

    const char *list = "[10, 20, 30]";
    CHECK(json_parse_document(list, text_length(list), &root));
    CHECK(!json_array_get(list, text_length(list), root, 3u, &value));
    CHECK(json_array_get(list, text_length(list), root, 0u, &value));
}

static void test_json_string_escapes(void) {
    const char *document = "\"a\\\"b\\\\c\\/d\\be\\ff\\ng\\rh\\ti\"";
    json_value root;
    CHECK(json_parse_document(document, text_length(document), &root));
    char output[64];
    uint64_t written = 0;
    CHECK(json_string_copy(document, text_length(document), root, output, sizeof output, &written));
    output[written] = '\0';
    CHECK_U64_EQ(written, 17u);
    CHECK_STR_EQ(output, "a\"b\\c/d\be\ff\ng\rh\ti");
}

static void test_json_string_unicode(void) {
    const char *latin = "\"\\u00e9\"";
    json_value root;
    CHECK(json_parse_document(latin, text_length(latin), &root));
    char output[16];
    uint64_t written = 0;
    CHECK(json_string_copy(latin, text_length(latin), root, output, sizeof output, &written));
    output[written] = '\0';
    CHECK_U64_EQ(written, 2u);
    CHECK_STR_EQ(output, "\xC3\xA9");

    const char *cjk = "\"\\u4e2d\"";
    CHECK(json_parse_document(cjk, text_length(cjk), &root));
    CHECK(json_string_copy(cjk, text_length(cjk), root, output, sizeof output, &written));
    output[written] = '\0';
    CHECK_U64_EQ(written, 3u);
    CHECK_STR_EQ(output, "\xE4\xB8\xAD");

    const char *astral = "\"\\ud83d\\ude00\"";
    CHECK(json_parse_document(astral, text_length(astral), &root));
    CHECK(json_string_copy(astral, text_length(astral), root, output, sizeof output, &written));
    output[written] = '\0';
    CHECK_U64_EQ(written, 4u);
    CHECK_STR_EQ(output, "\xF0\x9F\x98\x80");
}

static void test_json_string_invalid_escapes(void) {
    CHECK(!parse_ok("\"\\q\""));
    CHECK(!parse_ok("\"abc"));
    CHECK(!parse_ok("\"\\u12g4\""));

    const char *document = "\"\\udc00\"";
    json_value root;
    CHECK(json_parse_document(document, text_length(document), &root));
    char output[16];
    uint64_t written = 0;
    CHECK(!json_string_copy(document, text_length(document), root, output, sizeof output, &written));

    const char *high_without_low = "\"\\ud83dx\"";
    CHECK(json_parse_document(high_without_low, text_length(high_without_low), &root));
    CHECK(!json_string_copy(high_without_low, text_length(high_without_low), root, output, sizeof output, &written));

    const char *high_bad_low = "\"\\ud83d\\u1234\"";
    CHECK(json_parse_document(high_bad_low, text_length(high_bad_low), &root));
    CHECK(!json_string_copy(high_bad_low, text_length(high_bad_low), root, output, sizeof output, &written));

    const char *control = "\"a\nb\"";
    CHECK(!parse_ok(control));
}

static void test_json_string_capacity(void) {
    const char *document = "\"abcdef\"";
    json_value root;
    CHECK(json_parse_document(document, text_length(document), &root));
    char output[4];
    uint64_t written = 0;
    CHECK(!json_string_copy(document, text_length(document), root, output, sizeof output, &written));
}

static void test_json_number_forms(void) {
    const char *numbers[] = { "0", "123", "-5", "1.5", "1e3", "1E+3", "1e-3", "0.0" };
    for (uint32_t index = 0; index < (uint32_t)(sizeof numbers / sizeof numbers[0]); index += 1) {
        CHECK(parse_ok(numbers[index]));
    }
    CHECK(!parse_ok("-"));
    CHECK(!parse_ok("1."));
    CHECK(!parse_ok("1e"));
    CHECK(!parse_ok("+1"));

    json_value root;
    const char *document = "123";
    CHECK(json_parse_document(document, text_length(document), &root));
    uint64_t number = 0;
    CHECK(json_number_to_u64(document, text_length(document), root, &number));
    CHECK_U64_EQ(number, 123);

    const char *fraction = "1.5";
    CHECK(json_parse_document(fraction, text_length(fraction), &root));
    CHECK(!json_number_to_u64(fraction, text_length(fraction), root, &number));

    const char *string = "\"7\"";
    CHECK(json_parse_document(string, text_length(string), &root));
    CHECK(!json_number_to_u64(string, text_length(string), root, &number));

    const char *overflow = "99999999999999999999";
    CHECK(json_parse_document(overflow, text_length(overflow), &root));
    CHECK(!json_number_to_u64(overflow, text_length(overflow), root, &number));
}

static void test_json_malformed(void) {
    CHECK(!parse_ok("{"));
    CHECK(!parse_ok("[1, 2"));
    CHECK(!parse_ok("tru"));
    CHECK(!parse_ok("nul"));
    CHECK(!parse_ok("fals"));
    CHECK(!parse_ok("falsex"));
    CHECK(parse_ok("true"));
    CHECK(parse_ok("false"));
    CHECK(parse_ok("null"));

    const char *document = "{\"a\" 1}";
    json_value root;
    CHECK(json_parse_document(document, text_length(document), &root));
    json_value value;
    CHECK(!json_object_get(document, text_length(document), root, "a", &value));

    const char *non_string_key = "{1: 2}";
    CHECK(json_parse_document(non_string_key, text_length(non_string_key), &root));
    CHECK(!json_object_get(non_string_key, text_length(non_string_key), root, "1", &value));

    const char *missing_value = "{\"a\": }";
    CHECK(json_parse_document(missing_value, text_length(missing_value), &root));
    CHECK(!json_object_get(missing_value, text_length(missing_value), root, "a", &value));

    const char *no_comma = "{\"a\": 1 \"b\": 2}";
    CHECK(json_parse_document(no_comma, text_length(no_comma), &root));
    CHECK(!json_object_get(no_comma, text_length(no_comma), root, "b", &value));

    const char *array_no_comma = "[1 2]";
    CHECK(json_parse_document(array_no_comma, text_length(array_no_comma), &root));
    CHECK(!json_array_get(array_no_comma, text_length(array_no_comma), root, 1u, &value));
}

static void test_json_escaped_key_is_not_matched(void) {
    const char *document = "{\"a\\\\b\": 1}";
    json_value root;
    CHECK(json_parse_document(document, text_length(document), &root));
    json_value value;
    CHECK(!json_object_get(document, text_length(document), root, "a\\b", &value));
}

static void test_json_scan_value_at_offset(void) {
    const char *document = "first {\"a\": 1}";
    json_value value;
    CHECK(json_scan_value(document, text_length(document), 6u, &value));
    CHECK(value.kind == JSON_KIND_OBJECT);
    CHECK_U64_EQ(value.begin, 6u);
}


static void test_json_string_raw_utf8(void) {
    const char *raw = "\"a\xE2\x80\xA2\xC3\xA9\xF0\x9F\x98\x80\"";
    json_value root;
    CHECK(json_parse_document(raw, text_length(raw), &root));
    char output[32];
    uint64_t written = 0;
    CHECK(json_string_copy(raw, text_length(raw), root, output, sizeof output, &written));
    output[written] = '\0';
    CHECK_U64_EQ(written, 10u);
    CHECK(memcmp(output, "a\xE2\x80\xA2\xC3\xA9\xF0\x9F\x98\x80", 10u) == 0);

    CHECK(!json_string_copy(raw, text_length(raw), root, output, 9u, &written));
    CHECK(json_string_copy(raw, text_length(raw), root, output, 10u, &written));
    CHECK_U64_EQ(written, 10u);
}

void register_json_tests(void) {
    test_register("json_document_shape", test_json_document_shape);
    test_register("json_document_rejects_trailing_garbage", test_json_document_rejects_trailing_garbage);
    test_register("json_missing_members_and_elements", test_json_missing_members_and_elements);
    test_register("json_string_escapes", test_json_string_escapes);
    test_register("json_string_unicode", test_json_string_unicode);
    test_register("json_string_invalid_escapes", test_json_string_invalid_escapes);
    test_register("json_string_capacity", test_json_string_capacity);
    test_register("json_number_forms", test_json_number_forms);
    test_register("json_malformed", test_json_malformed);
    test_register("json_escaped_key_is_not_matched", test_json_escaped_key_is_not_matched);
    test_register("json_string_raw_utf8", test_json_string_raw_utf8);
    test_register("json_scan_value_at_offset", test_json_scan_value_at_offset);
}