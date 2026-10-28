#include "test.h"

#include "document.h"

#include <string.h>

#define TEST_MEMORY_BYTES (8u * 1024u * 1024u)
#define TEST_TEXT_BYTES (256u * 1024u)
#define TEST_PAGE_CAPACITY 8u

static uint8_t document_memory[TEST_MEMORY_BYTES];

static document *make_document(uint32_t pages) {
    static document instance;
    CHECK(document_init(&instance, document_memory, sizeof document_memory, pages, TEST_TEXT_BYTES));
    return &instance;
}

static const block *block_at(const document *doc, uint32_t page_index, uint32_t offset) {
    CHECK(page_index < doc->page_count);
    CHECK(offset < doc->pages[page_index].block_count);
    return &doc->blocks[doc->pages[page_index].first_block + offset];
}

static void check_block_text(const document *doc, const block *value, const char *expected) {
    uint64_t length = (uint64_t)strlen(expected);
    CHECK_U64_EQ(value->text_length, length);
    if (value->text_length == length) {
        CHECK(memcmp(document_block_text(doc, value), expected, (size_t)length) == 0);
    }
}

static void test_document_basic_page(void) {
    document *doc = make_document(TEST_PAGE_CAPACITY);
    const char *text = "139\n\nThe first paragraph\ncontinues here.\n\nThe second paragraph.\n\n139\n";
    CHECK(document_add_page(doc, 7u, text, (uint64_t)strlen(text)));
    CHECK_U64_EQ(doc->page_count, 1);
    const page *value = &doc->pages[0];
    CHECK_U64_EQ(value->source_page_index, 7);
    CHECK(value->has_printed_page_number);
    CHECK_U64_EQ(value->printed_page_number, 139);
    CHECK(!value->printed_is_roman);
    CHECK_U64_EQ(value->block_count, 4);
    CHECK_U64_EQ(block_at(doc, 0, 0)->kind, BLOCK_PAGE_NUMBER);
    CHECK_U64_EQ(block_at(doc, 0, 1)->kind, BLOCK_PARAGRAPH);
    check_block_text(doc, block_at(doc, 0, 1), "The first paragraph continues here.");
    check_block_text(doc, block_at(doc, 0, 2), "The second paragraph.");
    CHECK_U64_EQ(block_at(doc, 0, 3)->kind, BLOCK_PAGE_NUMBER);

    CHECK(document_finalize(doc, "fallback", NULL));
    CHECK_STR_EQ(document_title(doc), "fallback");
    CHECK_U64_EQ(doc->chapter_count, 1);
    CHECK(!doc->has_outline);
    CHECK_U64_EQ(doc->chapters[0].page_count, 1);
}

static void test_document_chapters(void) {
    document *doc = make_document(TEST_PAGE_CAPACITY);
    const char *first = "CHAPTER 1\n\nA prose sentence that ends with a period.\n";
    const char *second = "CHAPTER 2\n\nAnother prose sentence ending with a period.\n";
    CHECK(document_add_page(doc, 0, first, (uint64_t)strlen(first)));
    CHECK(document_add_page(doc, 1, second, (uint64_t)strlen(second)));
    CHECK(document_finalize(doc, "fallback", NULL));

    CHECK_U64_EQ(doc->chapter_count, 2);
    CHECK(doc->has_outline);
    CHECK_U64_EQ(doc->chapters[0].first_page, 0);
    CHECK_U64_EQ(doc->chapters[0].page_count, 1);
    CHECK_U64_EQ(doc->chapters[1].first_page, 1);
    CHECK_STR_EQ(document_title(doc), "CHAPTER 1");

    CHECK_U64_EQ(block_at(doc, 0, 0)->kind, BLOCK_HEADING);
    CHECK_U64_EQ(block_at(doc, 0, 0)->level, 1);
    CHECK_U64_EQ(block_at(doc, 1, 0)->level, 1);
}

static void test_document_demotes_second_level_one_heading(void) {
    document *doc = make_document(TEST_PAGE_CAPACITY);
    const char *text = "CHAPTER 1\n\nCHAPTER 2\n\nBody text.\n";
    CHECK(document_add_page(doc, 0, text, (uint64_t)strlen(text)));
    CHECK(document_finalize(doc, "fallback", NULL));
    CHECK_U64_EQ(doc->chapter_count, 1);
    CHECK_U64_EQ(block_at(doc, 0, 0)->level, 1);
    CHECK_U64_EQ(block_at(doc, 0, 1)->level, 2);
}

static void test_document_running_heads(void) {
    document *doc = make_document(TEST_PAGE_CAPACITY);
    const char *pages[] = {
        "139\n\nContents\n\nAlpha text.\n",
        "140\n\nContents\n\nBeta text.\n",
        "141\n\nContents\n\nGamma text.\n",
    };
    for (uint32_t index = 0; index < 3u; index += 1) {
        CHECK(document_add_page(doc, index, pages[index], (uint64_t)strlen(pages[index])));
    }
    CHECK(document_finalize(doc, "fallback", NULL));
    CHECK_U64_EQ(block_at(doc, 0, 1)->kind, BLOCK_PARAGRAPH);
    CHECK_U64_EQ(block_at(doc, 1, 1)->kind, BLOCK_RUNNING_HEAD);
    CHECK_U64_EQ(block_at(doc, 2, 1)->kind, BLOCK_RUNNING_HEAD);
}

static void test_document_footnotes(void) {
    document *doc = make_document(TEST_PAGE_CAPACITY);
    const char *text = "1 A numbered line that is first.\n\nSecond paragraph with a marker\xC2\xB9\xC2\xB2 in it.\n\nThird paragraph.\n\n12 A footnote at the end.\n";
    CHECK(document_add_page(doc, 0, text, (uint64_t)strlen(text)));
    CHECK(document_finalize(doc, "fallback", NULL));
    CHECK_U64_EQ(doc->pages[0].block_count, 4);
    CHECK_U64_EQ(block_at(doc, 0, 0)->kind, BLOCK_PARAGRAPH);
    CHECK_U64_EQ(block_at(doc, 0, 1)->kind, BLOCK_PARAGRAPH);
    CHECK_U64_EQ(block_at(doc, 0, 3)->kind, BLOCK_FOOTNOTE);
    CHECK_U64_EQ(block_at(doc, 0, 3)->footnote_number, 12);
    CHECK_U64_EQ(block_at(doc, 0, 3)->level, 0);
}

static void test_document_footnote_matched_by_superscript(void) {
    document *doc = make_document(TEST_PAGE_CAPACITY);
    const char *text = "7 A footnote early in the page.\n\nBody text with a reference\xE2\x81\xB7.\n\nMore body text.\n\nFinal body text.\n";
    CHECK(document_add_page(doc, 0, text, (uint64_t)strlen(text)));
    CHECK_U64_EQ(block_at(doc, 0, 0)->kind, BLOCK_FOOTNOTE);
    CHECK_U64_EQ(block_at(doc, 0, 0)->footnote_number, 7);
}

static void test_document_page_numbers_roman(void) {
    document *doc = make_document(TEST_PAGE_CAPACITY);
    const char *text = "xiv\n\nBody text here.\n\n139\n";
    CHECK(document_add_page(doc, 0, text, (uint64_t)strlen(text)));
    CHECK(doc->pages[0].has_printed_page_number);
    CHECK_U64_EQ(doc->pages[0].printed_page_number, 14);
    CHECK(doc->pages[0].printed_is_roman);
    CHECK_U64_EQ(block_at(doc, 0, 0)->kind, BLOCK_PAGE_NUMBER);
    CHECK_U64_EQ(block_at(doc, 0, 2)->kind, BLOCK_PAGE_NUMBER);
}

static void test_document_normalizes_lines(void) {
    document *doc = make_document(TEST_PAGE_CAPACITY);
    const char *text = "First line\r\nsecond line\r\n\r\nThird\tline\r\n";
    CHECK(document_add_page(doc, 0, text, (uint64_t)strlen(text)));
    CHECK_U64_EQ(doc->pages[0].block_count, 2);
    check_block_text(doc, block_at(doc, 0, 0), "First line second line");
    check_block_text(doc, block_at(doc, 0, 1), "Third line");
}

static void test_document_merges_parenthetical_subtitle(void) {
    document *doc = make_document(TEST_PAGE_CAPACITY);
    const char *text = "9. Chapter Title\n\n(A subtitle)\n\nBody text.\n";
    CHECK(document_add_page(doc, 0, text, (uint64_t)strlen(text)));
    CHECK_U64_EQ(doc->pages[0].block_count, 2);
    CHECK_U64_EQ(block_at(doc, 0, 0)->kind, BLOCK_HEADING);
    check_block_text(doc, block_at(doc, 0, 0), "9. Chapter Title (A subtitle)");
}

static void test_document_empty_page_and_title_override(void) {
    document *doc = make_document(TEST_PAGE_CAPACITY);
    CHECK(document_add_page(doc, 0, "", 0));
    CHECK_U64_EQ(doc->page_count, 1);
    CHECK_U64_EQ(doc->pages[0].block_count, 0);
    CHECK(document_finalize(doc, "fallback", "My Book"));
    CHECK_STR_EQ(document_title(doc), "My Book");
    CHECK_U64_EQ(doc->chapter_count, 1);
}

static void test_document_rejects_more_pages_than_capacity(void) {
    document *doc = make_document(1);
    CHECK(document_add_page(doc, 0, "one\n", 4));
    CHECK(!document_add_page(doc, 1, "two\n", 4));
}

static void test_document_rejects_block_overflow(void) {
    document *doc = make_document(1);
    char text[8192];
    uint64_t used = 0;
    for (uint32_t index = 0; index < 200u; index += 1) {
        const char *line = "Paragraph text.\n\n";
        uint64_t length = (uint64_t)strlen(line);
        if (used + length >= sizeof text) {
            break;
        }
        memcpy(text + used, line, (size_t)length);
        used += length;
    }
    CHECK(!document_add_page(doc, 0, text, used));
}

static void test_document_rejects_paragraph_overflow(void) {
    document *doc = make_document(1);
    static char text[4096];
    uint64_t used = 0;
    for (uint32_t index = 0; index < 600u; index += 1) {
        text[used] = 'x';
        used += 1u;
        text[used] = '\n';
        used += 1u;
        text[used] = '\n';
        used += 1u;
    }
    CHECK(!document_add_page(doc, 0, text, used));
}

static void test_document_rejects_text_overflow(void) {
    static document instance;
    CHECK(document_init(&instance, document_memory, sizeof document_memory, 1, 64));
    const char *text = "This paragraph is definitely too long to fit in sixty-four bytes of pool.\n";
    CHECK(!document_add_page(&instance, 0, text, (uint64_t)strlen(text)));
}

static void test_document_finalize_requires_pages(void) {
    document *doc = make_document(TEST_PAGE_CAPACITY);
    CHECK(!document_finalize(doc, "fallback", NULL));
}

static void test_document_classifies_tables_and_math(void) {
    document *doc = make_document(TEST_PAGE_CAPACITY);
    const char *text = "<table><tr><td>x</td></tr></table>\n\n$$x + y$$\n\nPlain text.\n";
    CHECK(document_add_page(doc, 0, text, (uint64_t)strlen(text)));
    CHECK_U64_EQ(block_at(doc, 0, 0)->kind, BLOCK_TABLE);
    CHECK_U64_EQ(block_at(doc, 0, 1)->kind, BLOCK_MATH);
    CHECK_U64_EQ(block_at(doc, 0, 2)->kind, BLOCK_PARAGRAPH);
}

static void document_fill_page(document *doc, uint32_t page_index, uint32_t paragraphs) {
    static char text[8192];
    uint64_t used = 0;
    for (uint32_t index = 0; index < paragraphs; index += 1) {
        const char *line = "Body text.\n\n";
        memcpy(text + used, line, strlen(line));
        used += (uint64_t)strlen(line);
    }
    CHECK(document_add_page(doc, page_index, text, used));
}

static void test_document_rejects_page_when_blocks_are_full(void) {
    /* Two pages share 256 blocks. A page that starts with a page number, a
     * footnote, a table, or a heading must still be refused when the block
     * storage is already full, rather than write past it. */
    const struct {
        const char *text;
    } cases[] = {
        { "139\n\nBody.\n" },
        { "1 note\xC2\xB9.\n\nBody.\n" },
        { "<table><tr><td>1</td></tr></table>\n\nBody.\n" },
        { "TITLE ONE\n\nBody text here.\n" },
    };
    for (uint32_t index = 0; index < (uint32_t)(sizeof cases / sizeof cases[0]); index += 1) {
        document *doc = make_document(2);
        document_fill_page(doc, 0, 256u);
        CHECK_U64_EQ(doc->block_count, 256u);
        CHECK(!document_add_page(doc, 1, cases[index].text, (uint64_t)strlen(cases[index].text)));
    }
}

static void test_document_rejects_heading_merge_overflow(void) {
    static document instance;
    CHECK(document_init(&instance, document_memory, sizeof document_memory, 2, 20));
    const char *text = "TITLE\n\n(a subtitle)\n\n";
    CHECK(!document_add_page(&instance, 0, text, (uint64_t)strlen(text)));
}

static void test_document_ignores_overlong_running_heads(void) {
    static char long_head[301];
    for (uint32_t index = 0; index + 10u < sizeof long_head; index += 10u) {
        memcpy(long_head + index, "long head ", 10u);
    }
    long_head[sizeof long_head - 1u] = '\0';

    document *doc = make_document(TEST_PAGE_CAPACITY);
    const char *pages[] = { "Contents", long_head, "Contents", "Contents" };
    const char *bodies[] = { "Alpha body.", "Beta body.", "Gamma body.", "Delta body." };
    for (uint32_t index = 0; index < 4u; index += 1) {
        char text[1024];
        int length = snprintf(text, sizeof text, "%s\n\n%s\n", pages[index], bodies[index]);
        CHECK(length > 0 && (uint64_t)length < sizeof text);
        CHECK(document_add_page(doc, index, text, (uint64_t)length));
    }
    CHECK(document_finalize(doc, "fallback", NULL));
    /* The three identical heads are running heads. The over-long page neither
     * matches them nor becomes one itself. */
    CHECK_U64_EQ(block_at(doc, 2, 0)->kind, BLOCK_RUNNING_HEAD);
    CHECK_U64_EQ(block_at(doc, 3, 0)->kind, BLOCK_RUNNING_HEAD);
    CHECK_U64_EQ(block_at(doc, 0, 0)->kind, BLOCK_PARAGRAPH);
    CHECK_U64_EQ(block_at(doc, 1, 0)->kind, BLOCK_PARAGRAPH);
}

static void test_document_running_heads_skip_blank_pages(void) {
    document *doc = make_document(TEST_PAGE_CAPACITY);
    const char *pages[] = { "Contents\n\nAlpha body.\n", "", "Contents\n\nBeta body.\n", "Contents\n\nGamma body.\n" };
    for (uint32_t index = 0; index < 4u; index += 1) {
        CHECK(document_add_page(doc, index, pages[index], (uint64_t)strlen(pages[index])));
    }
    CHECK(document_finalize(doc, "fallback", NULL));
    CHECK_U64_EQ(block_at(doc, 2, 0)->kind, BLOCK_RUNNING_HEAD);
    CHECK_U64_EQ(block_at(doc, 3, 0)->kind, BLOCK_RUNNING_HEAD);
    CHECK_U64_EQ(block_at(doc, 0, 0)->kind, BLOCK_PARAGRAPH);
}

static void test_document_truncates_long_titles(void) {
    static char long_text[600];
    memset(long_text, 't', sizeof long_text - 1u);
    long_text[sizeof long_text - 1u] = '\0';

    const char *pages[] = { "Body text.\n" };
    const uint32_t indices[] = { 0 };
    (void)indices;

    /* The --title option. */
    document *doc = make_document(TEST_PAGE_CAPACITY);
    CHECK(document_add_page(doc, 0, pages[0], (uint64_t)strlen(pages[0])));
    CHECK(document_finalize(doc, "fallback", long_text));
    CHECK_U64_EQ(doc->title_length, 511u);
    CHECK_U64_EQ((uint64_t)strlen(document_title(doc)), 511u);

    /* The file name fallback. */
    doc = make_document(TEST_PAGE_CAPACITY);
    CHECK(document_add_page(doc, 0, pages[0], (uint64_t)strlen(pages[0])));
    CHECK(document_finalize(doc, long_text, NULL));
    CHECK_U64_EQ(doc->title_length, 511u);

    /* A chapter heading merged with a long parenthetical subtitle. */
    static char page_text[1024];
    int length = snprintf(page_text, sizeof page_text, "CHAPTER 1\n\n(%.*s)\n\nBody text.\n", 600, long_text);
    CHECK(length > 0 && (uint64_t)length < sizeof page_text);
    doc = make_document(TEST_PAGE_CAPACITY);
    CHECK(document_add_page(doc, 0, page_text, (uint64_t)length));
    CHECK(document_finalize(doc, "fallback", NULL));
    CHECK_U64_EQ(doc->title_length, 511u);
}

static void test_document_title_must_fit_the_pool(void) {
    static document instance;
    CHECK(document_init(&instance, document_memory, sizeof document_memory, 1, 8));
    const char *text = "abcd\n";
    CHECK(document_add_page(&instance, 0, text, (uint64_t)strlen(text)));
    CHECK(!document_finalize(&instance, "xyzw", NULL));
}

static void test_document_lowercase_continuation_is_body_text(void) {
    /* The paragraph after a heading starts lowercase, so it continues the
     * heading's sentence and must not become a second heading. */
    document *doc = make_document(TEST_PAGE_CAPACITY);
    const char *text = "CHAPTER 1\n\ncontinued prose that follows.\n";
    CHECK(document_add_page(doc, 0, text, (uint64_t)strlen(text)));
    CHECK(document_finalize(doc, "fallback", NULL));
    CHECK_U64_EQ(block_at(doc, 0, 0)->kind, BLOCK_HEADING);
    CHECK_U64_EQ(block_at(doc, 0, 1)->kind, BLOCK_PARAGRAPH);
}

static void test_document_heading_at_page_end(void) {
    /* A heading that is the last paragraph on its page has no subtitle to
     * merge, and must still be classified as a heading. */
    document *doc = make_document(TEST_PAGE_CAPACITY);
    const char *text = "Body text.\n\nFINAL TITLE\n";
    CHECK(document_add_page(doc, 0, text, (uint64_t)strlen(text)));
    CHECK(document_finalize(doc, "fallback", NULL));
    CHECK_U64_EQ(block_at(doc, 0, 0)->kind, BLOCK_PARAGRAPH);
    CHECK_U64_EQ(block_at(doc, 0, 1)->kind, BLOCK_HEADING);
}

static void test_document_running_heads_stop_at_different_head(void) {
    document *doc = make_document(TEST_PAGE_CAPACITY);
    const char *pages[] = {
        "Contents\n\nAlpha body.\n",
        "Contents\n\nBeta body.\n",
        "Contents\n\nGamma body.\n",
        "Preface\n\nDelta body.\n",
    };
    for (uint32_t index = 0; index < 4u; index += 1) {
        CHECK(document_add_page(doc, index, pages[index], (uint64_t)strlen(pages[index])));
    }
    CHECK(document_finalize(doc, "fallback", NULL));
    CHECK_U64_EQ(block_at(doc, 1, 0)->kind, BLOCK_RUNNING_HEAD);
    CHECK_U64_EQ(block_at(doc, 2, 0)->kind, BLOCK_RUNNING_HEAD);
    CHECK_U64_EQ(block_at(doc, 3, 0)->kind, BLOCK_PARAGRAPH);
}

void register_document_tests(void) {
    test_register("document_basic_page", test_document_basic_page);
    test_register("document_chapters", test_document_chapters);
    test_register("document_lowercase_continuation_is_body_text", test_document_lowercase_continuation_is_body_text);
    test_register("document_demotes_second_level_one_heading", test_document_demotes_second_level_one_heading);
    test_register("document_running_heads", test_document_running_heads);
    test_register("document_footnotes", test_document_footnotes);
    test_register("document_footnote_matched_by_superscript", test_document_footnote_matched_by_superscript);
    test_register("document_page_numbers_roman", test_document_page_numbers_roman);
    test_register("document_normalizes_lines", test_document_normalizes_lines);
    test_register("document_merges_parenthetical_subtitle", test_document_merges_parenthetical_subtitle);
    test_register("document_empty_page_and_title_override", test_document_empty_page_and_title_override);
    test_register("document_rejects_more_pages_than_capacity", test_document_rejects_more_pages_than_capacity);
    test_register("document_rejects_block_overflow", test_document_rejects_block_overflow);
    test_register("document_rejects_paragraph_overflow", test_document_rejects_paragraph_overflow);
    test_register("document_rejects_text_overflow", test_document_rejects_text_overflow);
    test_register("document_finalize_requires_pages", test_document_finalize_requires_pages);
    test_register("document_classifies_tables_and_math", test_document_classifies_tables_and_math);
    test_register("document_rejects_page_when_blocks_are_full", test_document_rejects_page_when_blocks_are_full);
    test_register("document_rejects_heading_merge_overflow", test_document_rejects_heading_merge_overflow);
    test_register("document_ignores_overlong_running_heads", test_document_ignores_overlong_running_heads);
    test_register("document_running_heads_skip_blank_pages", test_document_running_heads_skip_blank_pages);
    test_register("document_heading_at_page_end", test_document_heading_at_page_end);
    test_register("document_running_heads_stop_at_different_head", test_document_running_heads_stop_at_different_head);
    test_register("document_truncates_long_titles", test_document_truncates_long_titles);
    test_register("document_title_must_fit_the_pool", test_document_title_must_fit_the_pool);
}