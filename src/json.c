#include "json.h"

#include "asserts.h"

#include <string.h>

static bool json_is_digit(char character) {
    return character >= '0' && character <= '9';
}

static bool json_is_hex_digit(char character) {
    return json_is_digit(character) || (character >= 'a' && character <= 'f') || (character >= 'A' && character <= 'F');
}

static uint32_t json_hex_value(char character) {
    if (character <= '9') {
        return (uint32_t)(character - '0');
    }
    if (character >= 'a') {
        return (uint32_t)(character - 'a') + 10u;
    }
    return (uint32_t)(character - 'A') + 10u;
}

static uint64_t json_skip_whitespace(const char *text, uint64_t length, uint64_t offset) {
    uint64_t index = offset;
    while (index < length) {
        char character = text[index];
        if (character != ' ' && character != '\t' && character != '\n' && character != '\r') {
            break;
        }
        index += 1;
    }
    return index;
}

static bool json_scan_string(const char *text, uint64_t length, uint64_t offset, uint64_t *end) {
    ASSERT(end != NULL);
    if (offset >= length || text[offset] != '"') {
        return false;
    }
    uint64_t index = offset + 1u;
    while (index < length) {
        char character = text[index];
        if (character == '\\') {
            if (index + 1u >= length) {
                return false;
            }
            char escape = text[index + 1u];
            if (escape == 'u') {
                if (index + 5u >= length) {
                    return false;
                }
                for (uint32_t digit = 0; digit < 4u; digit += 1) {
                    if (!json_is_hex_digit(text[index + 2u + digit])) {
                        return false;
                    }
                }
                index += 6u;
            } else if (escape == '"' || escape == '\\' || escape == '/' || escape == 'b' || escape == 'f' ||
                       escape == 'n' || escape == 'r' || escape == 't') {
                index += 2u;
            } else {
                return false;
            }
        } else if (character == '"') {
            *end = index + 1u;
            return true;
        } else if ((unsigned char)character < 0x20u) {
            return false;
        } else {
            index += 1u;
        }
    }
    return false;
}

static bool json_scan_container(const char *text, uint64_t length, uint64_t offset, char open, char close, uint64_t *end) {
    ASSERT(end != NULL);
    uint32_t depth = 0;
    uint64_t index = offset;
    while (index < length) {
        char character = text[index];
        if (character == '"') {
            uint64_t string_end = 0;
            if (!json_scan_string(text, length, index, &string_end)) {
                return false;
            }
            index = string_end;
            continue;
        }
        if (character == open) {
            depth += 1u;
        } else if (character == close) {
            depth -= 1u;
            if (depth == 0u) {
                *end = index + 1u;
                return true;
            }
        }
        index += 1u;
    }
    return false;
}

static bool json_scan_number(const char *text, uint64_t length, uint64_t offset, uint64_t *end) {
    uint64_t index = offset;
    if (index < length && text[index] == '-') {
        index += 1u;
    }
    if (index >= length) {
        return false;
    }
    if (text[index] == '0') {
        index += 1u;
    } else if (json_is_digit(text[index])) {
        while (index < length && json_is_digit(text[index])) {
            index += 1u;
        }
    } else {
        return false;
    }
    if (index < length && text[index] == '.') {
        index += 1u;
        if (index >= length || !json_is_digit(text[index])) {
            return false;
        }
        while (index < length && json_is_digit(text[index])) {
            index += 1u;
        }
    }
    if (index < length && (text[index] == 'e' || text[index] == 'E')) {
        index += 1u;
        if (index < length && (text[index] == '+' || text[index] == '-')) {
            index += 1u;
        }
        if (index >= length || !json_is_digit(text[index])) {
            return false;
        }
        while (index < length && json_is_digit(text[index])) {
            index += 1u;
        }
    }
    *end = index;
    return true;
}

static bool json_scan_literal(const char *text, uint64_t length, uint64_t offset, const char *literal, uint64_t *end) {
    uint64_t literal_length = (uint64_t)strlen(literal);
    if (offset + literal_length > length) {
        return false;
    }
    if (memcmp(text + offset, literal, (size_t)literal_length) != 0) {
        return false;
    }
    *end = offset + literal_length;
    return true;
}

bool json_scan_value(const char *text, uint64_t length, uint64_t offset, json_value *value) {
    ASSERT(text != NULL);
    ASSERT(value != NULL);

    uint64_t begin = json_skip_whitespace(text, length, offset);
    if (begin >= length) {
        return false;
    }
    uint64_t end = 0;
    switch (text[begin]) {
        case '"':
            value->kind = JSON_KIND_STRING;
            if (!json_scan_string(text, length, begin, &end)) {
                return false;
            }
            break;
        case '{':
            value->kind = JSON_KIND_OBJECT;
            if (!json_scan_container(text, length, begin, '{', '}', &end)) {
                return false;
            }
            break;
        case '[':
            value->kind = JSON_KIND_ARRAY;
            if (!json_scan_container(text, length, begin, '[', ']', &end)) {
                return false;
            }
            break;
        case 't':
            value->kind = JSON_KIND_BOOL;
            if (!json_scan_literal(text, length, begin, "true", &end)) {
                return false;
            }
            break;
        case 'f':
            value->kind = JSON_KIND_BOOL;
            if (!json_scan_literal(text, length, begin, "false", &end)) {
                return false;
            }
            break;
        case 'n':
            value->kind = JSON_KIND_NULL;
            if (!json_scan_literal(text, length, begin, "null", &end)) {
                return false;
            }
            break;
        default:
            value->kind = JSON_KIND_NUMBER;
            if (!json_scan_number(text, length, begin, &end)) {
                return false;
            }
            break;
    }
    value->begin = begin;
    value->end = end;
    return true;
}

static bool json_key_matches(const char *text, json_value key_value, const char *key) {
    uint64_t key_length = (uint64_t)strlen(key);
    if (key_value.end < key_value.begin + 2u) {
        return false;
    }
    uint64_t raw_length = key_value.end - key_value.begin - 2u;
    if (raw_length != key_length) {
        return false;
    }
    if (memcmp(text + key_value.begin + 1u, key, (size_t)key_length) != 0) {
        return false;
    }
    for (uint64_t index = 0; index < key_length; index += 1) {
        if (key[index] == '\\') {
            return false;
        }
    }
    return true;
}

bool json_object_get(const char *text, uint64_t length, json_value object, const char *key, json_value *value) {
    ASSERT(text != NULL);
    ASSERT(key != NULL);
    ASSERT(value != NULL);
    ASSERT(object.kind == JSON_KIND_OBJECT);

    uint64_t index = object.begin + 1u;
    while (index < length) {
        index = json_skip_whitespace(text, length, index);
        if (index >= length || text[index] == '}') {
            return false;
        }
        json_value member_key;
        if (!json_scan_value(text, length, index, &member_key)) {
            return false;
        }
        if (member_key.kind != JSON_KIND_STRING) {
            return false;
        }
        index = json_skip_whitespace(text, length, member_key.end);
        if (index >= length || text[index] != ':') {
            return false;
        }
        json_value member_value;
        if (!json_scan_value(text, length, index + 1u, &member_value)) {
            return false;
        }
        if (json_key_matches(text, member_key, key)) {
            *value = member_value;
            return true;
        }
        index = json_skip_whitespace(text, length, member_value.end);
        if (index >= length) {
            return false;
        }
        if (text[index] != ',') {
            return false;
        }
        index += 1u;
    }
    return false;
}

bool json_array_get(const char *text, uint64_t length, json_value array, uint64_t wanted, json_value *value) {
    ASSERT(text != NULL);
    ASSERT(value != NULL);
    ASSERT(array.kind == JSON_KIND_ARRAY);

    uint64_t index = array.begin + 1u;
    uint64_t current = 0;
    while (index < length) {
        index = json_skip_whitespace(text, length, index);
        if (index >= length || text[index] == ']') {
            return false;
        }
        json_value element;
        if (!json_scan_value(text, length, index, &element)) {
            return false;
        }
        if (current == wanted) {
            *value = element;
            return true;
        }
        current += 1u;
        index = json_skip_whitespace(text, length, element.end);
        if (index >= length) {
            return false;
        }
        if (text[index] != ',') {
            return false;
        }
        index += 1u;
    }
    return false;
}

static bool json_append_utf8(char *output, uint64_t capacity, uint64_t *used, uint32_t code_point) {
    uint32_t bytes = 1u;
    if (code_point >= 0x10000u) {
        bytes = 4u;
    } else if (code_point >= 0x800u) {
        bytes = 3u;
    } else if (code_point >= 0x80u) {
        bytes = 2u;
    }
    if (*used + (uint64_t)bytes > capacity) {
        return false;
    }
    if (bytes == 1u) {
        output[*used] = (char)code_point;
    } else if (bytes == 2u) {
        output[*used] = (char)(0xC0u | (code_point >> 6));
        output[*used + 1u] = (char)(0x80u | (code_point & 0x3Fu));
    } else if (bytes == 3u) {
        output[*used] = (char)(0xE0u | (code_point >> 12));
        output[*used + 1u] = (char)(0x80u | ((code_point >> 6) & 0x3Fu));
        output[*used + 2u] = (char)(0x80u | (code_point & 0x3Fu));
    } else {
        output[*used] = (char)(0xF0u | (code_point >> 18));
        output[*used + 1u] = (char)(0x80u | ((code_point >> 12) & 0x3Fu));
        output[*used + 2u] = (char)(0x80u | ((code_point >> 6) & 0x3Fu));
        output[*used + 3u] = (char)(0x80u | (code_point & 0x3Fu));
    }
    *used += (uint64_t)bytes;
    return true;
}

static uint32_t json_parse_hex4(const char *text, uint64_t offset) {
    uint32_t value = 0;
    for (uint32_t digit = 0; digit < 4u; digit += 1) {
        value = (value << 4) | json_hex_value(text[offset + digit]);
    }
    return value;
}

bool json_string_copy(const char *text, uint64_t length, json_value value, char *output, uint64_t output_capacity, uint64_t *written) {
    ASSERT(text != NULL);
    ASSERT(output != NULL);
    ASSERT(written != NULL);
    ASSERT(value.kind == JSON_KIND_STRING);
    ASSERT(value.end >= value.begin + 2u);
    ASSERT(value.end <= length);

    uint64_t index = value.begin + 1u;
    uint64_t end = value.end - 1u;
    uint64_t used = 0;
    bool ok = true;
    while (index < end && ok) {
        char character = text[index];
        if (character != '\\') {
            if (used + 1u > output_capacity) {
                return false;
            }
            output[used] = character;
            used += 1u;
            index += 1u;
            continue;
        }
        if (index + 1u >= end) {
            return false;
        }
        char escape = text[index + 1u];
        uint32_t code_point = 0;
        if (escape == 'u') {
            if (index + 5u >= end) {
                return false;
            }
            code_point = json_parse_hex4(text, index + 2u);
            if (code_point >= 0xD800u && code_point <= 0xDBFFu) {
                if (index + 11u >= end) {
                    return false;
                }
                if (text[index + 6u] != '\\' || text[index + 7u] != 'u') {
                    return false;
                }
                uint32_t low = json_parse_hex4(text, index + 8u);
                if (low < 0xDC00u || low > 0xDFFFu) {
                    return false;
                }
                code_point = 0x10000u + ((code_point - 0xD800u) << 10) + (low - 0xDC00u);
                index += 12u;
            } else if (code_point >= 0xDC00u && code_point <= 0xDFFFu) {
                return false;
            } else {
                index += 6u;
            }
        } else {
            if (escape == '"' || escape == '\\' || escape == '/') {
                code_point = (uint32_t)(unsigned char)escape;
            } else if (escape == 'b') {
                code_point = 0x08u;
            } else if (escape == 'f') {
                code_point = 0x0Cu;
            } else if (escape == 'n') {
                code_point = 0x0Au;
            } else if (escape == 'r') {
                code_point = 0x0Du;
            } else if (escape == 't') {
                code_point = 0x09u;
            } else {
                return false;
            }
            index += 2u;
        }
        ok = json_append_utf8(output, output_capacity, &used, code_point);
    }
    if (!ok) {
        return false;
    }
    *written = used;
    return true;
}

bool json_number_to_u64(const char *text, uint64_t length, json_value value, uint64_t *number) {
    ASSERT(text != NULL);
    ASSERT(number != NULL);
    ASSERT(value.begin <= value.end);
    ASSERT(value.end <= length);
    if (value.kind != JSON_KIND_NUMBER) {
        return false;
    }
    uint64_t result = 0;
    for (uint64_t index = value.begin; index < value.end; index += 1) {
        char character = text[index];
        if (!json_is_digit(character)) {
            return false;
        }
        uint64_t digit = (uint64_t)(character - '0');
        if (result > (UINT64_MAX - digit) / 10u) {
            return false;
        }
        result = result * 10u + digit;
    }
    *number = result;
    return true;
}

bool json_parse_document(const char *text, uint64_t length, json_value *value) {
    ASSERT(text != NULL);
    ASSERT(value != NULL);
    if (!json_scan_value(text, length, 0, value)) {
        return false;
    }
    uint64_t index = json_skip_whitespace(text, length, value->end);
    return index == length;
}