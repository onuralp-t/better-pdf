#ifndef BETTER_PDF_OCR_TEXT_H
#define BETTER_PDF_OCR_TEXT_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Shape analysis for raw OCR text.
 *
 * The model returns plain text: paragraphs separated by blank lines, numbers
 * written with Unicode superscripts for footnote references, and page numbers
 * on their own line. These helpers decide what each paragraph is. They are
 * pure functions over a byte span so the rules can be tested exhaustively
 * without any document, arena, or file.
 */

typedef struct {
    bool is_heading;
    uint8_t level;
    bool all_caps;
} ocr_heading_result;

typedef struct {
    bool present;
    bool roman;
    uint32_t number;
    uint64_t begin;
    uint64_t end;
} ocr_number_result;

bool ocr_next_paragraph(const char *text, uint64_t length, uint64_t offset, uint64_t *begin, uint64_t *end);
bool ocr_normalize_span(const char *text, uint64_t begin, uint64_t end, char *output, uint64_t capacity, uint64_t *written);
uint64_t ocr_normalize(const char *text, uint64_t length, char *output, uint64_t capacity);

bool ocr_number_token(const char *text, uint64_t length, ocr_number_result *result);
bool ocr_roman_value(const char *text, uint64_t length, uint32_t *value);
bool ocr_footnote_marker(const char *text, uint64_t length, uint32_t *marker, uint64_t *body_offset);
uint64_t ocr_strip_footnote_marker(const char *text, uint64_t length);
void ocr_classify_heading(const char *text, uint64_t length, bool first_content_block, bool next_starts_lowercase, ocr_heading_result *result);
bool ocr_is_parenthetical(const char *text, uint64_t length);
bool ocr_detect_table(const char *text, uint64_t length);
bool ocr_detect_math(const char *text, uint64_t length);
uint32_t ocr_word_count(const char *text, uint64_t length);
bool ocr_is_all_caps(const char *text, uint64_t length);

uint64_t ocr_utf8_decode(const char *text, uint64_t length, uint64_t offset, uint32_t *code_point);
uint32_t ocr_superscript_digit(uint32_t code_point);
uint64_t ocr_superscript_run(const char *text, uint64_t length, uint64_t offset, char *digits, uint64_t digit_capacity);
bool ocr_superscript_present(const char *text, uint64_t length, uint32_t number);

#endif