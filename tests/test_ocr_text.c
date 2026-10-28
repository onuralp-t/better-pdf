#include "test.h"

#include "ocr_text.h"

#include <string.h>

static uint64_t text_length(const char *text) {
    return (uint64_t)strlen(text);
}

static void test_ocr_next_paragraph(void) {
    const char *text = "\n\na\nb\n\n\nc\n";
    uint64_t begin = 0;
    uint64_t end = 0;
    CHECK(ocr_next_paragraph(text, text_length(text), 0, &begin, &end));
    CHECK_U64_EQ(begin, 2);
    CHECK_U64_EQ(end, 5);
    CHECK(ocr_next_paragraph(text, text_length(text), end, &begin, &end));
    CHECK_U64_EQ(begin, 8);
    CHECK_U64_EQ(end, 9);
    CHECK(!ocr_next_paragraph(text, text_length(text), end, &begin, &end));

    /* A blank line that ends at the end of the text has no newline to skip. */
    CHECK(!ocr_next_paragraph("   ", 3u, 0u, &begin, &end));

    const char *windows = "one\r\ntwo\r\n\r\nthree";
    CHECK(ocr_next_paragraph(windows, text_length(windows), 0, &begin, &end));
    CHECK_U64_EQ(end, 9);
    CHECK(ocr_next_paragraph(windows, text_length(windows), end, &begin, &end));
    CHECK_U64_EQ(begin, 12);
    CHECK_U64_EQ(end, 17);
}

static void test_ocr_normalize_span(void) {
    const char *text = "  a\t\tb  \n c ";
    char output[32];
    uint64_t written = 0;
    CHECK(ocr_normalize_span(text, 0, text_length(text), output, sizeof output, &written));
    output[written] = '\0';
    CHECK_U64_EQ(written, 5);
    CHECK_STR_EQ(output, "a b c");

    CHECK(!ocr_normalize_span(text, 0, text_length(text), output, 3, &written));
    CHECK(!ocr_normalize_span(text, 0, text_length(text), output, 1, &written));

    const char *spaces = "   \n\t ";
    CHECK(ocr_normalize_span(spaces, 0, text_length(spaces), output, sizeof output, &written));
    CHECK_U64_EQ(written, 0);
}

static void test_ocr_normalize_running_head(void) {
    const char *text = "Chapter 5: The 42nd Year";
    char output[64];
    uint64_t written = ocr_normalize(text, text_length(text), output, sizeof output);
    output[written] = '\0';
    CHECK_STR_EQ(output, "chapter #: the ##nd year");

    CHECK_U64_EQ(ocr_normalize("xy", 2, output, 1), 0);
}

static void test_ocr_roman(void) {
    uint32_t value = 0;
    CHECK(ocr_roman_value("xiv", 3, &value));
    CHECK_U64_EQ(value, 14);
    CHECK(ocr_roman_value("MCMXCIV", 7, &value));
    CHECK_U64_EQ(value, 1994);
    CHECK(!ocr_roman_value("ABC", 3, &value));
    CHECK(!ocr_roman_value("", 0, &value));
    CHECK(!ocr_roman_value("MMMMMMMMMMMMM", 13, &value));
}

static void test_ocr_number_token(void) {
    ocr_number_result result;
    CHECK(ocr_number_token("139", 3, &result));
    CHECK(result.present);
    CHECK(!result.roman);
    CHECK_U64_EQ(result.number, 139);

    CHECK(ocr_number_token(" xiv ", 5, &result));
    CHECK(result.roman);
    CHECK_U64_EQ(result.number, 14);

    CHECK(!ocr_number_token("12345", 5, &result));
    CHECK(!ocr_number_token("12a", 3, &result));
    CHECK(!ocr_number_token("", 0, &result));
    CHECK(!ocr_number_token("   ", 3, &result));
}

static void test_ocr_footnote_marker(void) {
    uint32_t marker = 0;
    uint64_t body = 0;
    CHECK(ocr_footnote_marker("180 Fleet, Trade, pp. 42-3.", text_length("180 Fleet, Trade, pp. 42-3."), &marker, &body));
    CHECK_U64_EQ(marker, 180);
    CHECK_U64_EQ(body, 4);

    CHECK(ocr_footnote_marker("12. A note", 10, &marker, &body));
    CHECK_U64_EQ(marker, 12);
    CHECK_U64_EQ(body, 4);

    CHECK(ocr_footnote_marker("3) Another", 10, &marker, &body));
    CHECK_U64_EQ(body, 3);

    CHECK(!ocr_footnote_marker("1234 too many", 13, &marker, &body));
    CHECK(!ocr_footnote_marker("12x", 3, &marker, &body));
    CHECK(!ocr_footnote_marker("12 ", 3, &marker, &body));
    CHECK(!ocr_footnote_marker("abc", 3, &marker, &body));

    CHECK_U64_EQ(ocr_strip_footnote_marker("7 Note text", 11), 2);
    CHECK_U64_EQ(ocr_strip_footnote_marker("no marker", 9), 0);
}

static void test_ocr_word_count_and_caps(void) {
    CHECK_U64_EQ(ocr_word_count("a b  c", 6), 3);
    CHECK_U64_EQ(ocr_word_count("", 0), 0);
    CHECK(ocr_is_all_caps("THE GREAT WAR", 13));
    CHECK(!ocr_is_all_caps("The Great War", 13));
    CHECK(!ocr_is_all_caps("123", 3));
    CHECK(!ocr_is_all_caps("A B", 3));
}

static void test_ocr_headings_chapter_markers(void) {
    ocr_heading_result result;
    ocr_classify_heading("CHAPTER 5", text_length("CHAPTER 5"), true, false, &result);
    CHECK(result.is_heading);
    CHECK_U64_EQ(result.level, 1);

    ocr_classify_heading("part two", 8, false, true, &result);
    CHECK(result.is_heading);
    CHECK_U64_EQ(result.level, 1);

    ocr_classify_heading("Book of Changes", 15, true, true, &result);
    CHECK(result.is_heading);
    CHECK_U64_EQ(result.level, 1);
}

static void test_ocr_headings_all_caps(void) {
    ocr_heading_result result;
    ocr_classify_heading("THE INDUSTRIAL REVOLUTION", 25, true, false, &result);
    CHECK(result.is_heading);
    CHECK(result.all_caps);
    CHECK_U64_EQ(result.level, 1);

    ocr_classify_heading("THE INDUSTRIAL REVOLUTION", 25, false, false, &result);
    CHECK_U64_EQ(result.level, 2);

    ocr_classify_heading("THE INDUSTRIAL REVOLUTION.", 26, true, false, &result);
    CHECK(!result.is_heading);
}

static void test_ocr_headings_numbered_and_section(void) {
    ocr_heading_result result;
    ocr_classify_heading("9. The Pragmatist's Progress: An Overview", text_length("9. The Pragmatist's Progress: An Overview"), false, true, &result);
    CHECK(result.is_heading);
    CHECK_U64_EQ(result.level, 2);

    ocr_classify_heading("\xC2\xA7" "131-134", 9, false, true, &result);
    CHECK(result.is_heading);
    CHECK_U64_EQ(result.level, 2);

    ocr_classify_heading("1234. Not a heading", 19, false, true, &result);
    CHECK(!result.is_heading);
}

static void test_ocr_headings_first_block_rule(void) {
    ocr_heading_result result;
    ocr_classify_heading("The Turkish economy, 1071-1453", text_length("The Turkish economy, 1071-1453"), true, false, &result);
    CHECK(result.is_heading);
    CHECK_U64_EQ(result.level, 2);

    ocr_classify_heading("The Turkish economy, 1071-1453.", 31, true, false, &result);
    CHECK(!result.is_heading);

    ocr_classify_heading("A Short Title", 13, true, true, &result);
    CHECK(!result.is_heading);

    ocr_classify_heading("Single", 6, true, false, &result);
    CHECK(!result.is_heading);

    ocr_classify_heading("Hyphenated-", 11, true, false, &result);
    CHECK(!result.is_heading);

    ocr_classify_heading("A rather long heading that keeps going well past the eighty character limit for sure", 90, true, false, &result);
    CHECK(!result.is_heading);

    /* A multi-word first paragraph ending in a hyphen is a broken word, not a
     * heading. */
    ocr_classify_heading("A Dash Heading -", text_length("A Dash Heading -"), true, false, &result);
    CHECK(!result.is_heading);
}

static void test_ocr_parenthetical_table_math(void) {
    CHECK(ocr_is_parenthetical("(a note)", 8));
    CHECK(ocr_is_parenthetical("  (a note)  ", 12));
    CHECK(!ocr_is_parenthetical("(unclosed", 9));
    CHECK(!ocr_is_parenthetical("()", 2));
    CHECK(ocr_is_parenthetical("( )", 3));

    CHECK(ocr_detect_table("<table><tr><td>x</td></tr></table>", 35));
    CHECK(ocr_detect_table("<TABLE>", 7));
    CHECK(!ocr_detect_table("table", 5));

    CHECK(ocr_detect_math("$$x$$", 5));
    CHECK(!ocr_detect_math("$$x$", 4));
    CHECK(!ocr_detect_math("$$", 2));
}

static void test_ocr_utf8(void) {
    uint32_t code_point = 0;
    CHECK_U64_EQ(ocr_utf8_decode("A", 1, 0, &code_point), 1);
    CHECK_U64_EQ(code_point, 'A');
    CHECK_U64_EQ(ocr_utf8_decode("\xC3\xA9", 2, 0, &code_point), 2);
    CHECK_U64_EQ(code_point, 0xE9);
    CHECK_U64_EQ(ocr_utf8_decode("\xE4\xB8\xAD", 3, 0, &code_point), 3);
    CHECK_U64_EQ(code_point, 0x4E2D);
    CHECK_U64_EQ(ocr_utf8_decode("\xF0\x9F\x98\x80", 4, 0, &code_point), 4);
    CHECK_U64_EQ(code_point, 0x1F600);
    CHECK_U64_EQ(ocr_utf8_decode("\xFF", 1, 0, &code_point), 0);
    CHECK_U64_EQ(ocr_utf8_decode("\xE4\xB8", 2, 0, &code_point), 0);
    CHECK_U64_EQ(ocr_utf8_decode("\xE4\x41\xAD", 3, 0, &code_point), 0);
    CHECK_U64_EQ(ocr_utf8_decode("\xC3\xA9", 2, 2, &code_point), 0);
}

static void test_ocr_superscripts(void) {
    CHECK_U64_EQ(ocr_superscript_digit(0x00B9), 1);
    CHECK_U64_EQ(ocr_superscript_digit(0x00B2), 2);
    CHECK_U64_EQ(ocr_superscript_digit(0x00B3), 3);
    CHECK_U64_EQ(ocr_superscript_digit(0x2070), 0);
    CHECK_U64_EQ(ocr_superscript_digit(0x2074), 4);
    CHECK_U64_EQ(ocr_superscript_digit(0x2079), 9);
    CHECK_U64_EQ(ocr_superscript_digit('A'), 10);

    const char *text = "\xC2\xB9\xE2\x81\xB8\xE2\x81\xB0";
    char digits[16];
    uint64_t run = ocr_superscript_run(text, text_length(text), 0, digits, sizeof digits - 1u);
    CHECK_U64_EQ(run, 8);
    CHECK_STR_EQ(digits, "180");

    CHECK_U64_EQ(ocr_superscript_run("abc", 3, 0, digits, sizeof digits - 1u), 0);

    CHECK(ocr_superscript_present("text\xC2\xB9\xE2\x81\xB8\xE2\x81\xB0 more", text_length("text\xC2\xB9\xE2\x81\xB8\xE2\x81\xB0 more"), 180));
    CHECK(!ocr_superscript_present("text\xC2\xB9\xE2\x81\xB8\xE2\x81\xB0 more", text_length("text\xC2\xB9\xE2\x81\xB8\xE2\x81\xB0 more"), 181));
    CHECK(!ocr_superscript_present("plain 180 digits", 16, 180));
    CHECK(ocr_superscript_present("\xC2\xB9\xE2\x81\xB4 x \xC2\xB2\xE2\x81\xB0\xE2\x81\xB0", text_length("\xC2\xB9\xE2\x81\xB4 x \xC2\xB2\xE2\x81\xB0\xE2\x81\xB0"), 200));
    /* An invalid UTF-8 byte is skipped one byte at a time. */
    CHECK(!ocr_superscript_present("\xFF", 1, 180));
}


static void test_ocr_normalize_joins_hyphenated_line_breaks(void) {
    const char *text = "exam-\nple stays well- known\nByzan-\ntine and pre-\nAnd";
    char output[96];
    uint64_t written = 0;
    CHECK(ocr_normalize_span(text, 0u, text_length(text), output, sizeof output, &written));
    output[written] = '\0';
    CHECK_STR_EQ(output, "example stays well- known Byzantine and pre- And");
}

static void test_ocr_normalize_line_break_edges(void) {
    /* A newline after a plain word keeps a space; a newline after an uppercase
     * letter keeps the hyphen; a newline after a non-letter keeps it too. */
    const char *text = "word\nbreak ab-\n{c";
    char output[64];
    uint64_t written = 0;
    CHECK(ocr_normalize_span(text, 0u, text_length(text), output, sizeof output, &written));
    output[written] = '\0';
    CHECK_STR_EQ(output, "word break ab- {c");
}

static void test_ocr_roman_all_letters(void) {
    uint32_t value = 0;
    CHECK(ocr_roman_value("l", 1, &value));
    CHECK_U64_EQ(value, 50);
    CHECK(ocr_roman_value("XL", 2, &value));
    CHECK_U64_EQ(value, 40);
    CHECK(ocr_roman_value("cd", 2, &value));
    CHECK_U64_EQ(value, 400);
}

static void test_ocr_footnote_marker_punctuation(void) {
    uint32_t marker = 0;
    uint64_t body = 0;
    CHECK(ocr_footnote_marker("7: Note", 7, &marker, &body));
    CHECK_U64_EQ(marker, 7);
    CHECK_U64_EQ(body, 3);

    /* Digits without the separating space are not a footnote. */
    CHECK(!ocr_footnote_marker("123", 3, &marker, &body));
    CHECK(!ocr_footnote_marker("12", 2, &marker, &body));
}

static void test_ocr_numbered_heading_roman_limits(void) {
    ocr_heading_result result;
    ocr_classify_heading("xii. The Long Voyage", 19, false, true, &result);
    CHECK(result.is_heading);
    CHECK_U64_EQ(result.level, 2);

    /* More than seven roman digits cannot start a heading. */
    ocr_classify_heading("iiiiiiii. Too Many", text_length("iiiiiiii. Too Many"), false, true, &result);
    CHECK(!result.is_heading);

    /* A lone number is not a heading, and neither is a number without a
     * following space. */
    ocr_classify_heading("1", 1, false, true, &result);
    CHECK(!result.is_heading);
    ocr_classify_heading("1.", 2, false, true, &result);
    CHECK(!result.is_heading);
    ocr_classify_heading("1.x", 3, false, true, &result);
    CHECK(!result.is_heading);

    /* A non-digit after three digits is not a number heading either. */
    ocr_classify_heading("123: x", 6, false, true, &result);
    CHECK(!result.is_heading);

    /* A roman prefix that reaches the end of the text has no dot. */
    ocr_classify_heading("xii", 3, false, true, &result);
    CHECK(!result.is_heading);
}

static void test_ocr_heading_length_limits(void) {
    ocr_heading_result result;
    ocr_classify_heading("", 0, true, false, &result);
    CHECK(!result.is_heading);

    static char long_caps[101];
    memset(long_caps, 'A', sizeof long_caps - 1u);
    long_caps[sizeof long_caps - 1u] = '\0';
    ocr_classify_heading(long_caps, 100, false, false, &result);
    CHECK(!result.is_heading);

    static char long_chapter[92];
    memcpy(long_chapter, "chapter ", 8);
    memset(long_chapter + 8, 'x', 80);
    long_chapter[88] = '\0';
    ocr_classify_heading(long_chapter, 88, false, true, &result);
    CHECK(!result.is_heading);

    /* Eleven words exceed the all-caps heading word budget. */
    ocr_classify_heading("A B C D E F G H I J K", 21, false, false, &result);
    CHECK(!result.is_heading);
}

static void test_ocr_heading_terminal_punctuation(void) {
    ocr_heading_result result;
    static const char *const endings[] = { ".", ",", ";", ":", "!", "?" };
    for (uint32_t index = 0; index < 6u; index += 1) {
        char text[32];
        int length = snprintf(text, sizeof text, "TITLE%s  ", endings[index]);
        CHECK(length > 0 && (uint64_t)length < sizeof text);
        ocr_classify_heading(text, (uint64_t)length, true, true, &result);
        CHECK(!result.is_heading);
    }
}

static void test_ocr_heading_section_limits(void) {
    ocr_heading_result result;
    ocr_classify_heading("\xC2", 1, false, true, &result);
    CHECK(!result.is_heading);
    ocr_classify_heading("\xC2\xB9", 2, false, true, &result);
    CHECK(!result.is_heading);

    static char long_section[42];
    long_section[0] = (char)0xC2;
    long_section[1] = (char)0xA7;
    memset(long_section + 2, 'x', 39);
    long_section[41] = '\0';
    ocr_classify_heading(long_section, 41, false, true, &result);
    CHECK(!result.is_heading);
}

static void test_ocr_all_caps_non_letters(void) {
    CHECK(ocr_is_all_caps("AB{CD", 5));
    CHECK(!ocr_is_all_caps("ab{", 3));
}

static void test_ocr_parenthetical_space_only(void) {
    CHECK(!ocr_is_parenthetical("   ", 3));
    CHECK(!ocr_is_parenthetical("", 0));
}

static void test_ocr_math_signature_edges(void) {
    CHECK(!ocr_detect_math("x$$$", 4));
    CHECK(!ocr_detect_math("$x$$", 4));
    CHECK(!ocr_detect_math("$$xy$", 5));
    CHECK(ocr_detect_math("$$$$", 4));
    CHECK(!ocr_detect_math("$$x$y", 5));
}

static void test_ocr_superscript_limits(void) {
    CHECK_U64_EQ(ocr_superscript_digit(0x206Fu), 10u);
    CHECK_U64_EQ(ocr_superscript_digit(0x207Au), 10u);
    char digits[16];
    const char *text = "\xC2\xB9\xC2\xB2";
    CHECK_U64_EQ(ocr_superscript_run(text, text_length(text), 0, digits, 1u), 0u);
    CHECK_U64_EQ(ocr_superscript_run(text, text_length(text), 0, digits, sizeof digits - 1u), 4u);
    CHECK_STR_EQ(digits, "12");
}

void register_ocr_text_tests(void) {
    test_register("ocr_next_paragraph", test_ocr_next_paragraph);
    test_register("ocr_normalize_span", test_ocr_normalize_span);
    test_register("ocr_normalize_joins_hyphenated_line_breaks", test_ocr_normalize_joins_hyphenated_line_breaks);
    test_register("ocr_normalize_running_head", test_ocr_normalize_running_head);
    test_register("ocr_roman", test_ocr_roman);
    test_register("ocr_number_token", test_ocr_number_token);
    test_register("ocr_footnote_marker", test_ocr_footnote_marker);
    test_register("ocr_word_count_and_caps", test_ocr_word_count_and_caps);
    test_register("ocr_headings_chapter_markers", test_ocr_headings_chapter_markers);
    test_register("ocr_headings_all_caps", test_ocr_headings_all_caps);
    test_register("ocr_headings_numbered_and_section", test_ocr_headings_numbered_and_section);
    test_register("ocr_headings_first_block_rule", test_ocr_headings_first_block_rule);
    test_register("ocr_parenthetical_table_math", test_ocr_parenthetical_table_math);
    test_register("ocr_utf8", test_ocr_utf8);
    test_register("ocr_superscripts", test_ocr_superscripts);
    test_register("ocr_normalize_line_break_edges", test_ocr_normalize_line_break_edges);
    test_register("ocr_roman_all_letters", test_ocr_roman_all_letters);
    test_register("ocr_footnote_marker_punctuation", test_ocr_footnote_marker_punctuation);
    test_register("ocr_numbered_heading_roman_limits", test_ocr_numbered_heading_roman_limits);
    test_register("ocr_heading_length_limits", test_ocr_heading_length_limits);
    test_register("ocr_heading_terminal_punctuation", test_ocr_heading_terminal_punctuation);
    test_register("ocr_heading_section_limits", test_ocr_heading_section_limits);
    test_register("ocr_all_caps_non_letters", test_ocr_all_caps_non_letters);
    test_register("ocr_parenthetical_space_only", test_ocr_parenthetical_space_only);
    test_register("ocr_math_signature_edges", test_ocr_math_signature_edges);
    test_register("ocr_superscript_limits", test_ocr_superscript_limits);
}