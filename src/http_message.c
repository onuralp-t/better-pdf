#include "http_message.h"

#include "asserts.h"

#include <stdio.h>
#include <string.h>

#define HTTP_REQUEST_HEADER_CAPACITY 1024u

static bool http_is_digit(uint8_t character) {
    return character >= '0' && character <= '9';
}

static char http_lower(char character) {
    if (character >= 'A' && character <= 'Z') {
        return (char)(character - 'A' + 'a');
    }
    return character;
}

static bool http_equals_ignore_case(const uint8_t *data, uint64_t length, const char *text) {
    uint64_t text_length = (uint64_t)strlen(text);
    if (length != text_length) {
        return false;
    }
    for (uint64_t index = 0; index < length; index += 1) {
        if (http_lower((char)data[index]) != http_lower(text[index])) {
            return false;
        }
    }
    return true;
}

static bool http_contains_ignore_case(const uint8_t *data, uint64_t length, const char *text) {
    uint64_t text_length = (uint64_t)strlen(text);
    if (text_length == 0 || text_length > length) {
        return false;
    }
    for (uint64_t start = 0; start + text_length <= length; start += 1) {
        if (http_equals_ignore_case(data + start, text_length, text)) {
            return true;
        }
    }
    return false;
}

uint64_t http_request_build(char *output, uint64_t output_capacity, const char *host, uint16_t port, const char *path, const char *body, uint64_t body_length) {
    ASSERT(output != NULL);
    ASSERT(host != NULL);
    ASSERT(path != NULL);
    ASSERT(body != NULL);

    char header[HTTP_REQUEST_HEADER_CAPACITY];
    int header_length = snprintf(header, sizeof header,
                                 "POST %s HTTP/1.1\r\n"
                                 "Host: %s:%u\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: %llu\r\n"
                                 "Connection: close\r\n"
                                 "\r\n",
                                 path, host, (unsigned)port, (unsigned long long)body_length);
    if (header_length < 0) {
        return 0;
    }
    if ((uint64_t)header_length >= (uint64_t)sizeof header) {
        return 0;
    }
    if ((uint64_t)header_length + body_length > output_capacity) {
        return 0;
    }
    memcpy(output, header, (uint64_t)header_length);
    if (body_length > 0) {
        memcpy(output + (uint64_t)header_length, body, (size_t)body_length);
    }
    return (uint64_t)header_length + body_length;
}

bool http_response_head_parse(const uint8_t *data, uint64_t length, http_response_head *head) {
    ASSERT(data != NULL);
    ASSERT(head != NULL);

    head->status_code = 0;
    head->has_content_length = false;
    head->content_length = 0;
    head->chunked = false;
    head->header_length = 0;

    uint64_t header_end = UINT64_MAX;
    for (uint64_t index = 0; index + 3u < length; index += 1) {
        if (data[index] == '\r' && data[index + 1u] == '\n' && data[index + 2u] == '\r' && data[index + 3u] == '\n') {
            header_end = index + 4u;
            break;
        }
    }
    if (header_end == UINT64_MAX) {
        return false;
    }

    uint64_t line_end = UINT64_MAX;
    for (uint64_t index = 0; index + 1u < header_end; index += 1) {
        if (data[index] == '\r' && data[index + 1u] == '\n') {
            line_end = index;
            break;
        }
    }
    if (line_end == UINT64_MAX) {
        return false;
    }

    uint64_t position = 0;
    if (line_end < 5u || memcmp(data, "HTTP/", 5u) != 0) {
        return false;
    }
    position = 5u;
    while (position < line_end && http_is_digit(data[position])) {
        position += 1u;
    }
    if (position >= line_end || data[position] != '.') {
        return false;
    }
    position += 1u;
    while (position < line_end && http_is_digit(data[position])) {
        position += 1u;
    }
    if (position >= line_end || data[position] != ' ') {
        return false;
    }
    position += 1u;
    if (position + 2u >= line_end) {
        return false;
    }
    if (!http_is_digit(data[position]) || !http_is_digit(data[position + 1u]) || !http_is_digit(data[position + 2u])) {
        return false;
    }
    head->status_code = (int32_t)((data[position] - '0') * 100 + (data[position + 1u] - '0') * 10 + (data[position + 2u] - '0'));

    uint64_t index = line_end + 2u;
    while (index < header_end) {
        uint64_t next = index;
        while (next + 1u < header_end && !(data[next] == '\r' && data[next + 1u] == '\n')) {
            next += 1u;
        }
        if (next + 1u >= header_end) {
            return false;
        }
        uint64_t header_length = next - index;
        if (header_length == 0u) {
            break;
        }
        uint64_t colon = UINT64_MAX;
        for (uint64_t cursor = index; cursor < next; cursor += 1) {
            if (data[cursor] == ':') {
                colon = cursor;
                break;
            }
        }
        if (colon == UINT64_MAX) {
            return false;
        }
        uint64_t name_length = colon - index;
        uint64_t value_begin = colon + 1u;
        while (value_begin < next && data[value_begin] == ' ') {
            value_begin += 1u;
        }
        uint64_t value_length = next - value_begin;

        if (http_equals_ignore_case(data + index, name_length, "content-length")) {
            if (value_length == 0u) {
                return false;
            }
            uint64_t content_length = 0;
            for (uint64_t cursor = value_begin; cursor < next; cursor += 1) {
                if (!http_is_digit(data[cursor])) {
                    return false;
                }
                content_length = content_length * 10u + (uint64_t)(data[cursor] - '0');
            }
            head->has_content_length = true;
            head->content_length = content_length;
        } else if (http_equals_ignore_case(data + index, name_length, "transfer-encoding")) {
            if (http_contains_ignore_case(data + value_begin, value_length, "chunked")) {
                head->chunked = true;
            }
        }
        index = next + 2u;
    }

    head->header_length = header_end;
    return true;
}

bool http_chunked_decode(const uint8_t *data, uint64_t length, uint8_t *output, uint64_t output_capacity, uint64_t *written) {
    ASSERT(data != NULL);
    ASSERT(output != NULL);
    ASSERT(written != NULL);

    uint64_t index = 0;
    uint64_t used = 0;
    for (;;) {
        uint64_t size = 0;
        bool digits = false;
        while (index < length) {
            uint8_t character = data[index];
            uint32_t digit = 0;
            if (http_is_digit(character)) {
                digit = (uint32_t)(character - '0');
            } else if (character >= 'a' && character <= 'f') {
                digit = (uint32_t)(character - 'a') + 10u;
            } else if (character >= 'A' && character <= 'F') {
                digit = (uint32_t)(character - 'A') + 10u;
            } else {
                break;
            }
            size = size * 16u + (uint64_t)digit;
            if (size > (uint64_t)UINT32_MAX) {
                return false;
            }
            digits = true;
            index += 1u;
        }
        if (!digits) {
            return false;
        }
        while (index < length && data[index] != '\r') {
            index += 1u;
        }
        if (index + 1u >= length || data[index] != '\r' || data[index + 1u] != '\n') {
            return false;
        }
        index += 2u;

        if (size == 0u) {
            for (;;) {
                if (index + 1u >= length) {
                    return false;
                }
                if (data[index] == '\r' && data[index + 1u] == '\n') {
                    *written = used;
                    return true;
                }
                while (index < length && data[index] != '\r') {
                    index += 1u;
                }
                if (index + 1u >= length || data[index] != '\r' || data[index + 1u] != '\n') {
                    return false;
                }
                index += 2u;
            }
        }
        if (size > output_capacity - used || index + size > length) {
            return false;
        }
        memcpy(output + used, data + index, (size_t)size);
        used += size;
        index += size;
        if (index + 1u >= length || data[index] != '\r' || data[index + 1u] != '\n') {
            return false;
        }
        index += 2u;
    }
}