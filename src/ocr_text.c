#include "ocr_text.h"

#include "asserts.h"

#include <string.h>

#define OCR_HEADING_MAX_CHARS 80u
#define OCR_NUMBERED_HEADING_MAX_CHARS 100u
#define OCR_SECTION_HEADING_MAX_CHARS 40u
#define OCR_HEADING_MAX_WORDS 10u
#define OCR_INK_PAGE_NUMBER_MAX_DIGITS 4u
#define OCR_ROMAN_MAX_CHARS 12u
#define OCR_FOOTNOTE_MAX_DIGITS 3u

static bool ocr_is_space(char character) {
    return character == ' ' || character == '\t' || character == '\r' || character == '\n';
}

static char ocr_lower(char character) {
    if (character >= 'A' && character <= 'Z') {
        return (char)(character - 'A' + 'a');
    }
    return character;
}

static bool ocr_line_is_blank(const char *text, uint64_t begin, uint64_t end) {
    ASSERT(text != NULL);
    ASSERT(begin <= end);
    for (uint64_t index = begin; index < end; index += 1) {
        if (!ocr_is_space(text[index])) {
            return false;
        }
    }
    return true;
}

bool ocr_next_paragraph(const char *text, uint64_t length, uint64_t offset, uint64_t *begin, uint64_t *end) {
    ASSERT(text != NULL);
    ASSERT(begin != NULL);
    ASSERT(end != NULL);
    ASSERT(offset <= length);

    uint64_t position = offset;
    for (;;) {
        if (position >= length) {
            return false;
        }
        uint64_t line_end = position;
        while (line_end < length && text[line_end] != '\n') {
            line_end += 1u;
        }
        if (!ocr_line_is_blank(text, position, line_end)) {
            break;
        }
        position = line_end < length ? line_end + 1u : length;
    }

    uint64_t paragraph_begin = position;
    uint64_t paragraph_end = position;
    while (position < length) {
        uint64_t line_end = position;
        while (line_end < length && text[line_end] != '\n') {
            line_end += 1u;
        }
        if (ocr_line_is_blank(text, position, line_end)) {
            break;
        }
        paragraph_end = line_end;
        position = line_end < length ? line_end + 1u : length;
    }
    *begin = paragraph_begin;
    *end = paragraph_end;
    return true;
}

bool ocr_normalize_span(const char *text, uint64_t begin, uint64_t end, char *output, uint64_t capacity, uint64_t *written) {
    ASSERT(text != NULL);
    ASSERT(output != NULL);
    ASSERT(written != NULL);
    ASSERT(begin <= end);

    uint64_t used = 0;
    bool pending_space = false;
    bool pending_line_break = false;
    for (uint64_t index = begin; index < end; index += 1) {
        char character = text[index];
        if (ocr_is_space(character)) {
            pending_space = used > 0u;
            if (character == '\n') {
                pending_line_break = true;
            }
            continue;
        }
        if (pending_space) {
            /* A word split by hyphenation at a hard line break arrives as
             * "exam-\nple"; joining it gives back "example". A hyphen followed
             * by an uppercase letter is kept, because that break is far more
             * likely to separate a real compound than to split a word. */
            /* pending_space is only set once a character has been written. */
            ASSERT(used > 0u);
            bool hyphenated = pending_line_break && output[used - 1u] == '-' && character >= 'a' && character <= 'z';
            if (hyphenated) {
                used -= 1u;
                pending_space = false;
            }
        }
        if (pending_space) {
            if (used >= capacity) {
                return false;
            }
            output[used] = ' ';
            used += 1u;
            pending_space = false;
        }
        pending_line_break = false;
        if (used >= capacity) {
            return false;
        }
        output[used] = character;
        used += 1u;
    }
    *written = used;
    return true;
}

uint64_t ocr_normalize(const char *text, uint64_t length, char *output, uint64_t capacity) {
    ASSERT(text != NULL);
    ASSERT(output != NULL);

    uint64_t written = 0;
    if (!ocr_normalize_span(text, 0, length, output, capacity, &written)) {
        return 0;
    }
    for (uint64_t index = 0; index < written; index += 1) {
        char character = ocr_lower(output[index]);
        if (character >= '0' && character <= '9') {
            output[index] = '#';
        } else {
            output[index] = character;
        }
    }
    return written;
}

static uint32_t ocr_roman_digit(char character) {
    switch (ocr_lower(character)) {
        case 'i':
            return 1u;
        case 'v':
            return 5u;
        case 'x':
            return 10u;
        case 'l':
            return 50u;
        case 'c':
            return 100u;
        case 'd':
            return 500u;
        case 'm':
            return 1000u;
        default:
            return 0u;
    }
}

bool ocr_roman_value(const char *text, uint64_t length, uint32_t *value) {
    ASSERT(text != NULL);
    ASSERT(value != NULL);

    if (length == 0u || length > OCR_ROMAN_MAX_CHARS) {
        return false;
    }
    uint32_t total = 0;
    for (uint64_t index = 0; index < length; index += 1) {
        uint32_t current = ocr_roman_digit(text[index]);
        if (current == 0u) {
            return false;
        }
        uint32_t next = 0;
        if (index + 1u < length) {
            next = ocr_roman_digit(text[index + 1u]);
        }
        if (next > current) {
            total -= current;
        } else {
            total += current;
        }
    }
    *value = total;
    return true;
}

bool ocr_number_token(const char *text, uint64_t length, ocr_number_result *result) {
    ASSERT(text != NULL);
    ASSERT(result != NULL);

    uint64_t begin = 0;
    uint64_t end = length;
    while (begin < end && ocr_is_space(text[begin])) {
        begin += 1u;
    }
    while (end > begin && ocr_is_space(text[end - 1u])) {
        end -= 1u;
    }
    if (begin == end) {
        return false;
    }
    uint64_t token_length = end - begin;

    bool digits = true;
    for (uint64_t index = begin; index < end && digits; index += 1) {
        if (text[index] < '0' || text[index] > '9') {
            digits = false;
        }
    }
    if (digits) {
        if (token_length > (uint64_t)OCR_INK_PAGE_NUMBER_MAX_DIGITS) {
            return false;
        }
        uint32_t number = 0;
        for (uint64_t index = begin; index < end; index += 1) {
            number = number * 10u + (uint32_t)(text[index] - '0');
        }
        result->present = true;
        result->roman = false;
        result->number = number;
        result->begin = begin;
        result->end = end;
        return true;
    }

    uint32_t roman = 0;
    if (!ocr_roman_value(text + begin, token_length, &roman)) {
        return false;
    }
    result->present = true;
    result->roman = true;
    result->number = roman;
    result->begin = begin;
    result->end = end;
    return true;
}

bool ocr_footnote_marker(const char *text, uint64_t length, uint32_t *marker, uint64_t *body_offset) {
    ASSERT(text != NULL);
    ASSERT(marker != NULL);
    ASSERT(body_offset != NULL);

    uint64_t index = 0;
    uint32_t number = 0;
    while (index < length && text[index] >= '0' && text[index] <= '9') {
        if (index >= (uint64_t)OCR_FOOTNOTE_MAX_DIGITS) {
            return false;
        }
        number = number * 10u + (uint32_t)(text[index] - '0');
        index += 1u;
    }
    if (index == 0u) {
        return false;
    }
    if (index < length && (text[index] == '.' || text[index] == ')' || text[index] == ':')) {
        index += 1u;
    }
    if (index >= length || text[index] != ' ') {
        return false;
    }
    while (index < length && text[index] == ' ') {
        index += 1u;
    }
    if (index >= length) {
        return false;
    }
    *marker = number;
    *body_offset = index;
    return true;
}

uint64_t ocr_strip_footnote_marker(const char *text, uint64_t length) {
    ASSERT(text != NULL);
    uint32_t marker = 0;
    uint64_t body_offset = 0;
    if (ocr_footnote_marker(text, length, &marker, &body_offset)) {
        return body_offset;
    }
    return 0;
}

uint32_t ocr_word_count(const char *text, uint64_t length) {
    ASSERT(text != NULL);
    uint32_t words = 0;
    bool in_word = false;
    for (uint64_t index = 0; index < length; index += 1) {
        if (ocr_is_space(text[index])) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            words += 1u;
        }
    }
    return words;
}

bool ocr_is_all_caps(const char *text, uint64_t length) {
    ASSERT(text != NULL);
    uint32_t letters = 0;
    for (uint64_t index = 0; index < length; index += 1) {
        char character = text[index];
        if (character >= 'a' && character <= 'z') {
            return false;
        }
        if (character >= 'A' && character <= 'Z') {
            letters += 1u;
        }
    }
    return letters >= 3u;
}

static bool ocr_is_chapter_marker(const char *text, uint64_t length) {
    ASSERT(text != NULL);
    static const char *markers[] = { "chapter", "part", "book" };
    for (uint32_t index = 0; index < 3u; index += 1) {
        uint64_t marker_length = (uint64_t)strlen(markers[index]);
        if (length <= marker_length) {
            continue;
        }
        if (ocr_lower(text[marker_length]) != ' ') {
            continue;
        }
        bool matches = true;
        for (uint64_t cursor = 0; cursor < marker_length && matches; cursor += 1) {
            if (ocr_lower(text[cursor]) != markers[index][cursor]) {
                matches = false;
            }
        }
        if (matches) {
            return true;
        }
    }
    return false;
}

static bool ocr_starts_with_numbered_heading(const char *text, uint64_t length) {
    /* ocr_classify_heading rejects empty text before calling this. */
    ASSERT(length > 0u);
    uint64_t index = 0;
    if (text[index] >= '0' && text[index] <= '9') {
        uint32_t digits = 0;
        while (index < length && text[index] >= '0' && text[index] <= '9' && digits < 3u) {
            index += 1u;
            digits += 1u;
        }
        if (index < length && text[index] >= '0' && text[index] <= '9') {
            return false;
        }
    } else {
        uint32_t roman_digits = 0;
        while (index < length && ocr_roman_digit(text[index]) != 0u && roman_digits < 7u) {
            index += 1u;
            roman_digits += 1u;
        }
        if (roman_digits == 0u) {
            return false;
        }
        if (index < length && ocr_roman_digit(text[index]) != 0u) {
            return false;
        }
    }
    if (index >= length || text[index] != '.') {
        return false;
    }
    index += 1u;
    return index < length && text[index] == ' ';
}

static bool ocr_ends_with_terminal_punctuation(const char *text, uint64_t length) {
    ASSERT(length > 0u);
    uint64_t end = length;
    /* Callers only ask about text with at least one word, so a non-space byte
     * is always ahead of the trim. */
    while (ocr_is_space(text[end - 1u])) {
        end -= 1u;
    }
    /* Callers only ask when the text has at least one word. */
    ASSERT(end > 0u);
    char last = text[end - 1u];
    return last == '.' || last == ',' || last == ';' || last == ':' || last == '!' || last == '?';
}

void ocr_classify_heading(const char *text, uint64_t length, bool first_content_block, bool next_starts_lowercase, ocr_heading_result *result) {
    ASSERT(text != NULL);
    ASSERT(result != NULL);

    result->is_heading = false;
    result->level = 0;
    result->all_caps = ocr_is_all_caps(text, length);

    if (length == 0u || length > OCR_NUMBERED_HEADING_MAX_CHARS) {
        return;
    }
    if (ocr_is_chapter_marker(text, length) && length <= OCR_HEADING_MAX_CHARS) {
        result->is_heading = true;
        result->level = 1;
        return;
    }
    if (result->all_caps && ocr_word_count(text, length) <= OCR_HEADING_MAX_WORDS &&
        length <= OCR_HEADING_MAX_CHARS && !ocr_ends_with_terminal_punctuation(text, length)) {
        result->is_heading = true;
        result->level = first_content_block ? 1u : 2u;
        return;
    }
    if (ocr_starts_with_numbered_heading(text, length)) {
        result->is_heading = true;
        result->level = 2;
        return;
    }
    if (text[0] == '\xC2' && length >= 2u && text[1] == '\xA7' && length <= OCR_SECTION_HEADING_MAX_CHARS) {
        result->is_heading = true;
        result->level = 2;
        return;
    }
    if (first_content_block && !next_starts_lowercase && length <= OCR_HEADING_MAX_CHARS &&
        ocr_word_count(text, length) >= 2u && !ocr_ends_with_terminal_punctuation(text, length)) {
        bool ends_with_hyphen = text[length - 1u] == '-';
        if (!ends_with_hyphen) {
            result->is_heading = true;
            result->level = 2;
        }
    }
}

bool ocr_is_parenthetical(const char *text, uint64_t length) {
    ASSERT(text != NULL);
    uint64_t begin = 0;
    uint64_t end = length;
    while (begin < end && ocr_is_space(text[begin])) {
        begin += 1u;
    }
    while (end > begin && ocr_is_space(text[end - 1u])) {
        end -= 1u;
    }
    if (end < begin + 3u) {
        return false;
    }
    return text[begin] == '(' && text[end - 1u] == ')';
}

static bool ocr_starts_with_ignore_case(const char *text, uint64_t length, const char *prefix) {
    ASSERT(text != NULL);
    ASSERT(prefix != NULL);
    uint64_t prefix_length = (uint64_t)strlen(prefix);
    if (length < prefix_length) {
        return false;
    }
    for (uint64_t index = 0; index < prefix_length; index += 1) {
        if (ocr_lower(text[index]) != ocr_lower(prefix[index])) {
            return false;
        }
    }
    return true;
}

bool ocr_detect_table(const char *text, uint64_t length) {
    ASSERT(text != NULL);
    return ocr_starts_with_ignore_case(text, length, "<table");
}

bool ocr_detect_math(const char *text, uint64_t length) {
    ASSERT(text != NULL);
    if (length < 4u) {
        return false;
    }
    return text[0] == '$' && text[1] == '$' && text[length - 2u] == '$' && text[length - 1u] == '$';
}

uint64_t ocr_utf8_decode(const char *text, uint64_t length, uint64_t offset, uint32_t *code_point) {
    ASSERT(text != NULL);
    ASSERT(code_point != NULL);
    if (offset >= length) {
        return 0;
    }
    unsigned char first = (unsigned char)text[offset];
    if (first < 0x80u) {
        *code_point = first;
        return 1u;
    }
    uint32_t bytes = 0;
    uint32_t value = 0;
    if ((first & 0xE0u) == 0xC0u) {
        bytes = 2u;
        value = first & 0x1Fu;
    } else if ((first & 0xF0u) == 0xE0u) {
        bytes = 3u;
        value = first & 0x0Fu;
    } else if ((first & 0xF8u) == 0xF0u) {
        bytes = 4u;
        value = first & 0x07u;
    } else {
        return 0;
    }
    if (offset + bytes > length) {
        return 0;
    }
    for (uint32_t index = 1; index < bytes; index += 1) {
        unsigned char continuation = (unsigned char)text[offset + index];
        if ((continuation & 0xC0u) != 0x80u) {
            return 0;
        }
        value = (value << 6) | (continuation & 0x3Fu);
    }
    *code_point = value;
    return bytes;
}

uint32_t ocr_superscript_digit(uint32_t code_point) {
    if (code_point == 0x00B9u) {
        return 1u;
    }
    if (code_point == 0x00B2u) {
        return 2u;
    }
    if (code_point == 0x00B3u) {
        return 3u;
    }
    if (code_point >= 0x2070u && code_point <= 0x2079u) {
        return code_point - 0x2070u;
    }
    return 10u;
}

uint64_t ocr_superscript_run(const char *text, uint64_t length, uint64_t offset, char *digits, uint64_t digit_capacity) {
    ASSERT(text != NULL);
    ASSERT(digits != NULL);

    uint64_t position = offset;
    uint64_t used = 0;
    for (;;) {
        uint32_t code_point = 0;
        uint64_t size = ocr_utf8_decode(text, length, position, &code_point);
        if (size == 0u) {
            break;
        }
        uint32_t digit = ocr_superscript_digit(code_point);
        if (digit > 9u) {
            break;
        }
        if (used >= digit_capacity) {
            return 0;
        }
        digits[used] = (char)('0' + digit);
        used += 1u;
        position += size;
    }
    if (used == 0u) {
        return 0;
    }
    digits[used] = '\0';
    return position - offset;
}

bool ocr_superscript_present(const char *text, uint64_t length, uint32_t number) {
    ASSERT(text != NULL);
    char digits[16];
    uint64_t offset = 0;
    while (offset < length) {
        uint64_t run = ocr_superscript_run(text, length, offset, digits, sizeof digits - 1u);
        if (run > 0u) {
            uint32_t value = 0;
            for (uint32_t index = 0; digits[index] != '\0'; index += 1) {
                value = value * 10u + (uint32_t)(digits[index] - '0');
            }
            if (value == number) {
                return true;
            }
            offset += run;
        } else {
            uint32_t code_point = 0;
            uint64_t size = ocr_utf8_decode(text, length, offset, &code_point);
            offset += size == 0u ? 1u : size;
        }
    }
    return false;
}