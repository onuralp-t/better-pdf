#include "test.h"

#include "http_message.h"

#include <string.h>

static uint64_t text_length(const char *text) {
    return (uint64_t)strlen(text);
}

static void test_http_request_build(void) {
    char output[512];
    const char *body = "{\"x\":1}";
    uint64_t written = http_request_build(output, sizeof output, "127.0.0.1", 18080, "/v1/chat/completions", body, text_length(body));
    CHECK(written > 0u);
    output[written] = '\0';
    CHECK(strncmp(output, "POST /v1/chat/completions HTTP/1.1\r\n", 36u) == 0);
    CHECK(strstr(output, "Host: 127.0.0.1:18080\r\n") != NULL);
    CHECK(strstr(output, "Content-Type: application/json\r\n") != NULL);
    CHECK(strstr(output, "Content-Length: 7\r\n") != NULL);
    CHECK(strstr(output, "Connection: close\r\n") != NULL);
    CHECK(strstr(output, "\r\n\r\n{\"x\":1}") != NULL);
}

static void test_http_request_build_empty_body(void) {
    char output[512];
    uint64_t written = http_request_build(output, sizeof output, "localhost", 80, "/", "", 0);
    CHECK(written > 0u);
    CHECK(strstr(output, "Content-Length: 0\r\n") != NULL);
    CHECK(output[written - 1u] == '\n');
}

static void test_http_request_build_capacity(void) {
    char output[64];
    CHECK_U64_EQ(http_request_build(output, sizeof output, "127.0.0.1", 8080, "/v1/chat/completions", "{}", 2), 0u);
}

static void test_http_request_build_long_path(void) {
    static char path[2048];
    memset(path, 'p', sizeof path - 1u);
    path[0] = '/';
    path[sizeof path - 1u] = '\0';
    char output[64];
    CHECK_U64_EQ(http_request_build(output, sizeof output, "127.0.0.1", 8080, path, "{}", 2), 0u);
}

static void test_http_head_content_length(void) {
    const char *response = "HTTP/1.1 200 OK\r\nContent-Length: 42\r\n\r\n";
    http_response_head head;
    CHECK(http_response_head_parse((const uint8_t *)response, text_length(response), &head));
    CHECK_U64_EQ((uint64_t)head.status_code, 200u);
    CHECK(head.has_content_length);
    CHECK_U64_EQ(head.content_length, 42u);
    CHECK(!head.chunked);
    CHECK_U64_EQ(head.header_length, text_length(response));
}

static void test_http_head_chunked(void) {
    const char *response = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
    http_response_head head;
    CHECK(http_response_head_parse((const uint8_t *)response, text_length(response), &head));
    CHECK(head.chunked);
    CHECK(!head.has_content_length);

    const char *upper = "HTTP/1.1 200 OK\r\nTRANSFER-ENCODING: CHUNKED\r\n\r\n";
    CHECK(http_response_head_parse((const uint8_t *)upper, text_length(upper), &head));
    CHECK(head.chunked);

    const char *other = "HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip\r\n\r\n";
    CHECK(http_response_head_parse((const uint8_t *)other, text_length(other), &head));
    CHECK(!head.chunked);
}

static void test_http_head_lowercase_names_and_status(void) {
    const char *response = "HTTP/1.1 404 Not Found\r\ncontent-length: 0\r\n\r\n";
    http_response_head head;
    CHECK(http_response_head_parse((const uint8_t *)response, text_length(response), &head));
    CHECK_U64_EQ((uint64_t)head.status_code, 404u);
    CHECK(head.has_content_length);
    CHECK_U64_EQ(head.content_length, 0u);
}

static void test_http_head_rejects_malformed(void) {
    http_response_head head;
    const char *no_terminator = "HTTP/1.1 200 OK\r\nContent-Length: 1\r\n";
    CHECK(!http_response_head_parse((const uint8_t *)no_terminator, text_length(no_terminator), &head));

    const char *no_version = "HTTP 200 OK\r\n\r\n";
    CHECK(!http_response_head_parse((const uint8_t *)no_version, text_length(no_version), &head));

    const char *bad_version = "HTTP/1 200 OK\r\n\r\n";
    CHECK(!http_response_head_parse((const uint8_t *)bad_version, text_length(bad_version), &head));

    const char *short_code = "HTTP/1.1 20\r\n\r\n";
    CHECK(!http_response_head_parse((const uint8_t *)short_code, text_length(short_code), &head));

    const char *non_digit_code = "HTTP/1.1 2a0 OK\r\n\r\n";
    CHECK(!http_response_head_parse((const uint8_t *)non_digit_code, text_length(non_digit_code), &head));

    const char *no_colon = "HTTP/1.1 200 OK\r\nBogus\r\n\r\n";
    CHECK(!http_response_head_parse((const uint8_t *)no_colon, text_length(no_colon), &head));

    const char *empty_length = "HTTP/1.1 200 OK\r\nContent-Length: \r\n\r\n";
    CHECK(!http_response_head_parse((const uint8_t *)empty_length, text_length(empty_length), &head));

    const char *bad_length = "HTTP/1.1 200 OK\r\nContent-Length: 12a\r\n\r\n";
    CHECK(!http_response_head_parse((const uint8_t *)bad_length, text_length(bad_length), &head));
}

static void test_http_chunked_decode_simple(void) {
    const char *body = "5\r\nhello\r\n0\r\n\r\n";
    uint8_t output[32];
    uint64_t written = 0;
    CHECK(http_chunked_decode((const uint8_t *)body, text_length(body), output, sizeof output, &written));
    CHECK_U64_EQ(written, 5u);
    CHECK(memcmp(output, "hello", 5u) == 0);
}

static void test_http_chunked_decode_multiple_and_extension(void) {
    const char *body = "5;name=value\r\nhello\r\n6\r\n world\r\n0\r\n\r\n";
    uint8_t output[32];
    uint64_t written = 0;
    CHECK(http_chunked_decode((const uint8_t *)body, text_length(body), output, sizeof output, &written));
    CHECK_U64_EQ(written, 11u);
    CHECK(memcmp(output, "hello world", 11u) == 0);
}

static void test_http_chunked_decode_empty(void) {
    const char *body = "0\r\n\r\n";
    uint8_t output[8];
    uint64_t written = 1;
    CHECK(http_chunked_decode((const uint8_t *)body, text_length(body), output, sizeof output, &written));
    CHECK_U64_EQ(written, 0u);
}

static void test_http_chunked_decode_trailer(void) {
    const char *body = "2\r\nhi\r\n0\r\nX-Trace: 1\r\n\r\n";
    uint8_t output[8];
    uint64_t written = 0;
    CHECK(http_chunked_decode((const uint8_t *)body, text_length(body), output, sizeof output, &written));
    CHECK_U64_EQ(written, 2u);
    CHECK(memcmp(output, "hi", 2u) == 0);
}

static void test_http_chunked_decode_rejects_malformed(void) {
    uint8_t output[32];
    uint64_t written = 0;

    const char *no_size = "z\r\nhello\r\n0\r\n\r\n";
    CHECK(!http_chunked_decode((const uint8_t *)no_size, text_length(no_size), output, sizeof output, &written));

    const char *overflow = "FFFFFFFFF\r\n";
    CHECK(!http_chunked_decode((const uint8_t *)overflow, text_length(overflow), output, sizeof output, &written));

    const char *no_size_terminator = "5\r\nhello\r\n0\r\n";
    CHECK(!http_chunked_decode((const uint8_t *)no_size_terminator, text_length(no_size_terminator), output, sizeof output, &written));

    const char *truncated_data = "5\r\nhel";
    CHECK(!http_chunked_decode((const uint8_t *)truncated_data, text_length(truncated_data), output, sizeof output, &written));

    const char *no_data_terminator = "2\r\nhiX";
    CHECK(!http_chunked_decode((const uint8_t *)no_data_terminator, text_length(no_data_terminator), output, sizeof output, &written));

    const char *no_trailer_terminator = "0\r\nX-Trace: 1\r\n";
    CHECK(!http_chunked_decode((const uint8_t *)no_trailer_terminator, text_length(no_trailer_terminator), output, sizeof output, &written));

    const char *small_capacity = "5\r\nhello\r\n0\r\n\r\n";
    CHECK(!http_chunked_decode((const uint8_t *)small_capacity, text_length(small_capacity), output, 4u, &written));

    const char *size_without_crlf = "5";
    CHECK(!http_chunked_decode((const uint8_t *)size_without_crlf, text_length(size_without_crlf), output, sizeof output, &written));
}


static void test_http_head_uppercase_and_unchunked(void) {
    const char *response =
        "HTTP/1.0 404 Not Found\r\n"
        "CONTENT-LENGTH: 0\r\n"
        "TRANSFER-ENCODING: gzipped\r\n"
        "\r\n";
    http_response_head head;
    CHECK(http_response_head_parse((const uint8_t *)response, text_length(response), &head));
    CHECK_U64_EQ(head.status_code, 404u);
    CHECK(head.has_content_length);
    CHECK_U64_EQ(head.content_length, 0u);
    CHECK(!head.chunked);
}

static void test_http_head_rejects_bad_status_lines(void) {
    http_response_head head;
    const char *short_line = "H\r\n\r\n";
    CHECK(!http_response_head_parse((const uint8_t *)short_line, text_length(short_line), &head));
    const char *no_minor = "HTTP/11\r\n\r\n";
    CHECK(!http_response_head_parse((const uint8_t *)no_minor, text_length(no_minor), &head));
    const char *no_space = "HTTP/1.1X200 OK\r\n\r\n";
    CHECK(!http_response_head_parse((const uint8_t *)no_space, text_length(no_space), &head));
    const char *bad_first = "HTTP/1.1 a00 OK\r\n\r\n";
    CHECK(!http_response_head_parse((const uint8_t *)bad_first, text_length(bad_first), &head));
    const char *bad_third = "HTTP/1.1 20a OK\r\n\r\n";
    CHECK(!http_response_head_parse((const uint8_t *)bad_third, text_length(bad_third), &head));
    const char *lowercase = "http/1.1 200 OK\r\n\r\n";
    CHECK(!http_response_head_parse((const uint8_t *)lowercase, text_length(lowercase), &head));
}

static void test_http_chunked_decode_uppercase_hex(void) {
    const char *body = "A\r\n0123456789\r\n0\r\n\r\n";
    uint8_t output[32];
    uint64_t written = 0;
    CHECK(http_chunked_decode((const uint8_t *)body, text_length(body), output, sizeof output, &written));
    CHECK_U64_EQ(written, 10u);
    CHECK(memcmp(output, "0123456789", 10u) == 0);
}

static void test_http_head_malformed_line_endings(void) {
    http_response_head head;

    /* A carriage return followed by a normal byte inside the code is not a
     * line ending, and it also breaks the status code. */
    const char *stray_cr = "HTTP/1.1 2\r0 OK\r\n\r\n";
    CHECK(!http_response_head_parse((const uint8_t *)stray_cr, text_length(stray_cr), &head));

    /* CR LF CR followed by anything but LF does not end the header block. */
    const char *partial_terminator = "HTTP/1.1 200 OK\r\n\rX\r\n\r\n";
    CHECK(!http_response_head_parse((const uint8_t *)partial_terminator, text_length(partial_terminator), &head));

    /* The version ends right after the dot, so no minor digits exist. */
    const char *empty_minor = "HTTP/1.\r\n\r\n";
    CHECK(!http_response_head_parse((const uint8_t *)empty_minor, text_length(empty_minor), &head));
}

static void test_http_head_stray_cr_in_header_line(void) {
    /* A lone carriage return inside a header line is value data, not a break. */
    const char *response = "HTTP/1.1 200 OK\r\nX-Data: one\r two\r\n\r\n";
    http_response_head head;
    CHECK(http_response_head_parse((const uint8_t *)response, text_length(response), &head));
    CHECK_U64_EQ((uint64_t)head.status_code, 200u);
    CHECK(!head.has_content_length);
    CHECK_U64_EQ(head.header_length, text_length(response));
}

static void test_http_chunked_decode_lowercase_hex(void) {
    const char *body = "a\r\n0123456789\r\n0\r\n\r\n";
    uint8_t output[32];
    uint64_t written = 0;
    CHECK(http_chunked_decode((const uint8_t *)body, text_length(body), output, sizeof output, &written));
    CHECK_U64_EQ(written, 10u);
    CHECK(memcmp(output, "0123456789", 10u) == 0);
}

static void test_http_chunked_decode_bad_size_terminators(void) {
    uint8_t output[32];
    uint64_t written = 0;

    /* The size must be followed by CR LF, not by a stray CR. */
    const char *no_lf = "5\rX\r\nhello\r\n0\r\n\r\n";
    CHECK(!http_chunked_decode((const uint8_t *)no_lf, text_length(no_lf), output, sizeof output, &written));

    /* Trailer lines must also end with CR LF. */
    const char *trailer_stray_cr = "0\r\n\rX\r\n\r\n";
    CHECK(!http_chunked_decode((const uint8_t *)trailer_stray_cr, text_length(trailer_stray_cr), output, sizeof output, &written));
    const char *trailer_unterminated = "0\r\nXY";
    CHECK(!http_chunked_decode((const uint8_t *)trailer_unterminated, text_length(trailer_unterminated), output, sizeof output, &written));

    /* Chunk data must be followed by CR LF; a stray byte is caught even when
     * more input follows it. */
    const char *data_stray = "2\r\nhiXYZ";
    CHECK(!http_chunked_decode((const uint8_t *)data_stray, text_length(data_stray), output, sizeof output, &written));
    const char *data_stray_cr = "2\r\nhi\rXYZ";
    CHECK(!http_chunked_decode((const uint8_t *)data_stray_cr, text_length(data_stray_cr), output, sizeof output, &written));
}

void register_http_message_tests(void) {
    test_register("http_request_build", test_http_request_build);
    test_register("http_request_build_empty_body", test_http_request_build_empty_body);
    test_register("http_request_build_capacity", test_http_request_build_capacity);
    test_register("http_request_build_long_path", test_http_request_build_long_path);
    test_register("http_head_content_length", test_http_head_content_length);
    test_register("http_head_chunked", test_http_head_chunked);
    test_register("http_head_lowercase_names_and_status", test_http_head_lowercase_names_and_status);
    test_register("http_head_rejects_malformed", test_http_head_rejects_malformed);
    test_register("http_chunked_decode_simple", test_http_chunked_decode_simple);
    test_register("http_chunked_decode_multiple_and_extension", test_http_chunked_decode_multiple_and_extension);
    test_register("http_chunked_decode_empty", test_http_chunked_decode_empty);
    test_register("http_chunked_decode_trailer", test_http_chunked_decode_trailer);
    test_register("http_chunked_decode_rejects_malformed", test_http_chunked_decode_rejects_malformed);
    test_register("http_head_uppercase_and_unchunked", test_http_head_uppercase_and_unchunked);
    test_register("http_head_rejects_bad_status_lines", test_http_head_rejects_bad_status_lines);
    test_register("http_chunked_decode_uppercase_hex", test_http_chunked_decode_uppercase_hex);
    test_register("http_head_malformed_line_endings", test_http_head_malformed_line_endings);
    test_register("http_head_stray_cr_in_header_line", test_http_head_stray_cr_in_header_line);
    test_register("http_chunked_decode_lowercase_hex", test_http_chunked_decode_lowercase_hex);
    test_register("http_chunked_decode_bad_size_terminators", test_http_chunked_decode_bad_size_terminators);
}