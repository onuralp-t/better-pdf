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


static void test_json_malformed_strings(void) {
    json_value root;
    CHECK(!json_parse_document("\"unterminated", 13u, &root));
    CHECK(!json_parse_document("\"bad\\", 5u, &root));
    CHECK(!json_parse_document("\"bad\\u12\"", text_length("\"bad\\u12\""), &root));
    CHECK(!json_parse_document("\"bad\\uZZZZ\"", text_length("\"bad\\uZZZZ\""), &root));
    CHECK(!json_parse_document("\"bad\\q\"", 7u, &root));
    CHECK(!json_parse_document("\"bad\x01\"", 6u, &root));

    const char *lone_high = "\"\\ud83d\"";
    CHECK(json_parse_document(lone_high, text_length(lone_high), &root));
    char output[16];
    uint64_t written = 0;
    CHECK(!json_string_copy(lone_high, text_length(lone_high), root, output, sizeof output, &written));

    const char *lone_low = "\"\\ude00\"";
    CHECK(json_parse_document(lone_low, text_length(lone_low), &root));
    CHECK(!json_string_copy(lone_low, text_length(lone_low), root, output, sizeof output, &written));

    const char *bad_pair = "\"\\ud83d\\u0041\"";
    CHECK(json_parse_document(bad_pair, text_length(bad_pair), &root));
    CHECK(!json_string_copy(bad_pair, text_length(bad_pair), root, output, sizeof output, &written));

}

static void test_json_malformed_numbers_and_literals(void) {
    json_value root;
    CHECK(!json_parse_document("-", 1u, &root));
    CHECK(!json_parse_document("1.", 2u, &root));
    CHECK(!json_parse_document("1e", 2u, &root));
    CHECK(!json_parse_document("1e+", 3u, &root));
    CHECK(!json_parse_document("1.x", 3u, &root));
    CHECK(!json_parse_document("1.2x", 4u, &root));
    CHECK(!json_parse_document("1e+x", 4u, &root));
    CHECK(!json_parse_document("1e5x", 4u, &root));
    CHECK(!json_parse_document("01", 2u, &root));
    CHECK(!json_parse_document("+1", 2u, &root));
    CHECK(!json_parse_document("tru", 3u, &root));
    CHECK(!json_parse_document("falsey", 6u, &root));
    CHECK(!json_parse_document("nul", 3u, &root));
    CHECK(!json_parse_document("{", 1u, &root));
    CHECK(!json_parse_document("[", 1u, &root));
    CHECK(!json_parse_document("\"a\" \"b\"", 7u, &root));
    CHECK(json_parse_document(" 1 ", 3u, &root));
    CHECK_U64_EQ(root.kind, JSON_KIND_NUMBER);
}

static void test_json_malformed_members(void) {
    json_value root;
    const char *empty_object = "{}";
    CHECK(json_parse_document(empty_object, 2u, &root));
    json_value member;
    CHECK(!json_object_get(empty_object, 2u, root, "a", &member));

    const char *missing_colon = "{\"a\" 1}";
    CHECK(json_parse_document(missing_colon, 7u, &root));
    CHECK(!json_object_get(missing_colon, 7u, root, "a", &member));

    const char *missing_comma = "{\"a\":1 \"b\":2}";
    CHECK(json_parse_document(missing_comma, 13u, &root));
    CHECK(!json_object_get(missing_comma, 13u, root, "b", &member));

    const char *non_string_key = "{1:2}";
    CHECK(json_parse_document(non_string_key, 5u, &root));
    CHECK(!json_object_get(non_string_key, 5u, root, "1", &member));

    const char *bad_member_value = "{\"a\":}";
    CHECK(json_parse_document(bad_member_value, 6u, &root));
    CHECK(!json_object_get(bad_member_value, 6u, root, "a", &member));

    const char *empty_array = "[]";
    CHECK(json_parse_document(empty_array, 2u, &root));
    json_value element;
    CHECK(!json_array_get(empty_array, 2u, root, 0u, &element));

    const char *bad_array_value = "[1,]";
    CHECK(json_parse_document(bad_array_value, 4u, &root));
    CHECK(!json_array_get(bad_array_value, 4u, root, 1u, &element));

    const char *missing_array_comma = "[1 2]";
    CHECK(json_parse_document(missing_array_comma, 5u, &root));
    CHECK(!json_array_get(missing_array_comma, 5u, root, 1u, &element));

    const char *escaped_key = "{\"a\\u0062\":1}";
    CHECK(json_parse_document(escaped_key, 13u, &root));
    CHECK(!json_object_get(escaped_key, 13u, root, "ab", &member));
}

static void test_json_uppercase_hex_escape(void) {
    const char *document = "\"\\u00AF\"";
    json_value root;
    CHECK(json_parse_document(document, text_length(document), &root));
    char output[8];
    uint64_t written = 0;
    CHECK(json_string_copy(document, text_length(document), root, output, sizeof output, &written));
    output[written] = '\0';
    CHECK_U64_EQ(written, 2u);
    CHECK(memcmp(output, "\xC2\xAF", 2u) == 0);
}

static void test_json_astral_capacity(void) {
    const char *document = "\"\\ud83d\\ude00\"";
    json_value root;
    CHECK(json_parse_document(document, text_length(document), &root));
    char output[8];
    uint64_t written = 0;
    CHECK(!json_string_copy(document, text_length(document), root, output, 3u, &written));
    CHECK(json_string_copy(document, text_length(document), root, output, 4u, &written));
    CHECK_U64_EQ(written, 4u);
}

static void test_json_malformed_containers(void) {
    CHECK(!parse_ok("{\"a"));
    CHECK(!parse_ok("truf"));
    CHECK(!parse_ok("[1"));
    CHECK(!parse_ok("{\"a\":1,"));
}

static void test_json_object_get_tail_cases(void) {
    json_value root;
    json_value value;

    /* The raw key text matches, but it holds an escape and is never decoded. */
    const char *escaped_key = "{\"a\\u0062\":1}";
    CHECK(json_parse_document(escaped_key, text_length(escaped_key), &root));
    CHECK(!json_object_get(escaped_key, text_length(escaped_key), root, "a\\u0062", &value));

    /* A truncated document still yields a value region, and the accessor must
     * stop at the end it was given instead of reading past it. */
    const char *trailing_comma = "{\"a\":1,";
    root.kind = JSON_KIND_OBJECT;
    root.begin = 0;
    root.end = text_length(trailing_comma);
    CHECK(!json_object_get(trailing_comma, text_length(trailing_comma), root, "b", &value));

    const char *missing_close = "{\"a\":1";
    root.begin = 0;
    root.end = text_length(missing_close);
    CHECK(!json_object_get(missing_close, text_length(missing_close), root, "b", &value));
}

static void test_json_array_get_tail_cases(void) {
    json_value root;
    json_value value;

    const char *trailing_comma = "[1,";
    root.kind = JSON_KIND_ARRAY;
    root.begin = 0;
    root.end = text_length(trailing_comma);
    CHECK(!json_array_get(trailing_comma, text_length(trailing_comma), root, 1u, &value));

    const char *double_comma = "[1,,2]";
    CHECK(json_parse_document(double_comma, text_length(double_comma), &root));
    CHECK(!json_array_get(double_comma, text_length(double_comma), root, 1u, &value));

    const char *unterminated = "[1";
    root.begin = 0;
    root.end = text_length(unterminated);
    CHECK(!json_array_get(unterminated, text_length(unterminated), root, 1u, &value));
}

static void test_json_string_copy_truncated_escapes(void) {
    char output[16];
    uint64_t written = 0;
    json_value value;

    /* A backslash immediately before the closing quote escapes it. */
    const char *trailing_backslash = "\"abc\\\"";
    value.kind = JSON_KIND_STRING;
    value.begin = 0;
    value.end = text_length(trailing_backslash);
    CHECK(!json_string_copy(trailing_backslash, text_length(trailing_backslash), value, output, sizeof output, &written));

    /* A unicode escape with fewer than four hex digits inside the region. */
    const char *short_unicode = "\"a\\u12\"";
    value.begin = 0;
    value.end = text_length(short_unicode);
    CHECK(!json_string_copy(short_unicode, text_length(short_unicode), value, output, sizeof output, &written));

    /* A high surrogate that is not followed by a low surrogate escape. */
    const char *lonely_high = "\"\\uD83Dabcdefgh\"";
    CHECK(json_parse_document(lonely_high, text_length(lonely_high), &value));
    CHECK(!json_string_copy(lonely_high, text_length(lonely_high), value, output, sizeof output, &written));

    /* An escape letter that JSON does not define. */
    const char *bad_escape = "\"a\\q\"";
    value.begin = 0;
    value.end = text_length(bad_escape);
    CHECK(!json_string_copy(bad_escape, text_length(bad_escape), value, output, sizeof output, &written));
}

static void test_json_object_get_rejects_nonstring_key(void) {
    /* The member does not even start with a string, so scanning it fails. */
    const char *text = "{,";
    json_value root;
    root.kind = JSON_KIND_OBJECT;
    root.begin = 0;
    root.end = text_length(text);
    json_value value;
    CHECK(!json_object_get(text, text_length(text), root, "x", &value));
}

static void test_json_hex_rejects_other_characters(void) {
    /* A character below ''A'' and one above ''F'' are both non-hex digits. */
    CHECK(!parse_ok("\"\\u00/0\""));
    CHECK(!parse_ok("\"\\u00G0\""));
}

static void test_json_surrogate_range_edges(void) {
    const char *beyond_low = "\"\\uD83D\\uE000\"";
    json_value root;
    CHECK(json_parse_document(beyond_low, text_length(beyond_low), &root));
    char output[16];
    uint64_t written = 0;
    CHECK(!json_string_copy(beyond_low, text_length(beyond_low), root, output, sizeof output, &written));

    const char *private_use = "\"\\uE000\"";
    CHECK(json_parse_document(private_use, text_length(private_use), &root));
    CHECK(json_string_copy(private_use, text_length(private_use), root, output, sizeof output, &written));
    CHECK_U64_EQ(written, 3u);
    CHECK(memcmp(output, "\xEE\x80\x80", 3u) == 0);

    /* A high surrogate followed by a backslash that does not start a low one. */
    const char *escaped_low = "\"a\\uD83D\\qpppp\"";
    json_value manual;
    manual.kind = JSON_KIND_STRING;
    manual.begin = 0;
    manual.end = text_length(escaped_low);
    CHECK(!json_string_copy(escaped_low, text_length(escaped_low), manual, output, sizeof output, &written));
}

static void test_json_string_capacity_stops_midway(void) {
    const char *document = "\"abcdef\"";
    json_value root;
    CHECK(json_parse_document(document, text_length(document), &root));
    char output[2];
    uint64_t written = 0;
    CHECK(!json_string_copy(document, text_length(document), root, output, sizeof output, &written));

    /* A unicode escape needs more room than is left: the loop notices the
     * failure while there is still input to scan. */
    const char *escaped = "\"a\\u0041b\"";
    CHECK(json_parse_document(escaped, text_length(escaped), &root));
    char single[1];
    CHECK(!json_string_copy(escaped, text_length(escaped), root, single, sizeof single, &written));
}

static void test_json_accessors_stop_at_the_end(void) {
    json_value root;
    json_value value;

    /* The object is empty but ends before any closing brace is seen. */
    const char *empty_object = "{ ";
    root.kind = JSON_KIND_OBJECT;
    root.begin = 0;
    root.end = text_length(empty_object);
    CHECK(!json_object_get(empty_object, text_length(empty_object), root, "x", &value));

    /* The key ends exactly at the end of the text, with no colon after it. */
    const char *key_only = "{\"a\"";
    root.begin = 0;
    root.end = text_length(key_only);
    CHECK(!json_object_get(key_only, text_length(key_only), root, "x", &value));

    const char *empty_array = "[ ";
    root.kind = JSON_KIND_ARRAY;
    root.begin = 0;
    root.end = text_length(empty_array);
    CHECK(!json_array_get(empty_array, text_length(empty_array), root, 0u, &value));
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
    test_register("json_malformed_strings", test_json_malformed_strings);
    test_register("json_malformed_numbers_and_literals", test_json_malformed_numbers_and_literals);
    test_register("json_malformed_members", test_json_malformed_members);
    test_register("json_scan_value_at_offset", test_json_scan_value_at_offset);
    test_register("json_uppercase_hex_escape", test_json_uppercase_hex_escape);
    test_register("json_astral_capacity", test_json_astral_capacity);
    test_register("json_malformed_containers", test_json_malformed_containers);
    test_register("json_object_get_tail_cases", test_json_object_get_tail_cases);
    test_register("json_array_get_tail_cases", test_json_array_get_tail_cases);
    test_register("json_string_copy_truncated_escapes", test_json_string_copy_truncated_escapes);
    test_register("json_hex_rejects_other_characters", test_json_hex_rejects_other_characters);
    test_register("json_surrogate_range_edges", test_json_surrogate_range_edges);
    test_register("json_string_capacity_stops_midway", test_json_string_capacity_stops_midway);
    test_register("json_accessors_stop_at_the_end", test_json_accessors_stop_at_the_end);
    test_register("json_object_get_rejects_nonstring_key", test_json_object_get_rejects_nonstring_key);
}