#include "test.h"

#include "document.h"
#include "geometry.h"
#include "typst_emit.h"

#include <string.h>

#define TEST_MEMORY_BYTES (8u * 1024u * 1024u)
#define TEST_TEXT_BYTES (256u * 1024u)

static uint8_t typst_memory[TEST_MEMORY_BYTES];
static char typst_output[512u * 1024u];
static document typst_document;

static document *build_document(const char *const *pages, const uint32_t *page_indices, uint32_t count, const char *title_override) {
    CHECK(document_init(&typst_document, typst_memory, sizeof typst_memory, 8u, TEST_TEXT_BYTES));
    for (uint32_t index = 0; index < count; index += 1) {
        CHECK(document_add_page(&typst_document, page_indices[index], pages[index], (uint64_t)strlen(pages[index])));
    }
    CHECK(document_finalize(&typst_document, "book", title_override));
    return &typst_document;
}

static uint64_t emit(const document *doc, bool include_outline) {
    typst_config config;
    config.geometry.page_width_pt10 = 4308u;
    config.geometry.page_height_pt10 = 6462u;
    config.geometry.margin_x_pt10 = 560u;
    config.geometry.margin_y_pt10 = 646u;
    config.geometry.body_pt10 = 105u;
    config.title = document_title(doc);
    config.title_length = doc->title_length;
    config.include_outline = include_outline;
    uint64_t written = typst_emit(doc, &config, typst_output, sizeof typst_output);
    if (written < sizeof typst_output) {
        typst_output[written] = '\0';
    }
    return written;
}

static bool contains(const char *needle) {
    return strstr(typst_output, needle) != NULL;
}

static uint64_t count_occurrences(const char *needle) {
    uint64_t count = 0;
    const char *found = typst_output;
    while ((found = strstr(found, needle)) != NULL) {
        count += 1u;
        found += 1;
    }
    return count;
}

static void test_typst_header_and_paragraph(void) {
    const char *pages[] = { "Hello world.\n" };
    const uint32_t indices[] = { 0 };
    const document *doc = build_document(pages, indices, 1, "A Book");
    CHECK(emit(doc, false) > 0u);
    CHECK(contains("#set document(title: \"A Book\")"));
    CHECK(contains("#set page(width: 430.8pt, height: 646.2pt, margin: (x: 56.0pt, y: 64.6pt), numbering: \"1\")"));
    CHECK(contains("#set text(font: \"Libertinus Serif\", size: 10.5pt)"));
    CHECK(contains("Hello world."));
    CHECK(!contains("#outline"));
    CHECK(!contains("#pagebreak"));
}

static void test_typst_escapes_markup(void) {
    const char *pages[] = { "#1 $2$ *bold* _it_ `raw` <label> @ref ~ [x] a\\b\n\n- leading dash\n" };
    const uint32_t indices[] = { 0 };
    const document *doc = build_document(pages, indices, 1, "book");
    CHECK(emit(doc, false) > 0u);
    CHECK(contains("\\#1 \\$2\\$ \\*bold\\* \\_it\\_ \\`raw\\` \\<label\\> \\@ref \\~ \\[x\\] a\\\\b"));
    CHECK(contains("\\- leading dash"));
}

static void test_typst_chapters_and_outline(void) {
    const char *pages[] = {
        "CHAPTER 1\n\nFirst chapter prose that ends with a period.\n",
        "CHAPTER 2\n\nSecond chapter prose that ends with a period.\n",
    };
    const uint32_t indices[] = { 0, 1 };
    const document *doc = build_document(pages, indices, 2, NULL);
    CHECK(emit(doc, true) > 0u);
    CHECK(contains("#outline(title: [Contents], indent: 1.2em)"));
    CHECK(contains("#heading(level: 1)[CHAPTER 1]"));
    CHECK(contains("#heading(level: 1)[CHAPTER 2]"));
    CHECK(contains("#pagebreak(weak: true)"));
}

static void test_typst_no_outline_when_disabled(void) {
    const char *pages[] = {
        "CHAPTER 1\n\nFirst chapter prose that ends with a period.\n",
        "CHAPTER 2\n\nSecond chapter prose that ends with a period.\n",
    };
    const uint32_t indices[] = { 0, 1 };
    const document *doc = build_document(pages, indices, 2, NULL);
    CHECK(emit(doc, false) > 0u);
    CHECK(!contains("#outline"));
    CHECK(contains("#pagebreak(weak: true)"));
}

static void test_typst_footnotes(void) {
    const char *pages[] = { "Body text with a reference\xC2\xB9\xC2\xB2.\n\n12 The note body.\n" };
    const uint32_t indices[] = { 0 };
    const document *doc = build_document(pages, indices, 1, "book");
    CHECK(emit(doc, false) > 0u);
    CHECK(contains("#footnote[The note body.]"));
    CHECK(!contains("\xC2\xB9\xC2\xB2"));
}

static void test_typst_superscript_without_footnote(void) {
    const char *pages[] = { "The second power x\xC2\xB2 is here.\n" };
    const uint32_t indices[] = { 0 };
    const document *doc = build_document(pages, indices, 1, "book");
    CHECK(emit(doc, false) > 0u);
    CHECK(contains("#super[2]"));
}

static void test_typst_unmatched_footnote_is_kept(void) {
    const char *pages[] = { "Body text that ends here.\n\nMore body text.\n\nThird body text.\n\n3 A note with no reference.\n" };
    const uint32_t indices[] = { 0 };
    const document *doc = build_document(pages, indices, 1, "book");
    CHECK(emit(doc, false) > 0u);
    CHECK(contains("#footnote[A note with no reference.]"));
}

static void test_typst_math_and_table(void) {
    const char *pages[] = { "$$a + b$$\n\n<table><tr><td>1</td></tr></table>\n" };
    const uint32_t indices[] = { 0 };
    const document *doc = build_document(pages, indices, 1, "book");
    CHECK(emit(doc, false) > 0u);
    CHECK(contains("#align(center)[$ a + b $]"));
    CHECK(contains("\\<table\\>"));
}

static void test_typst_skips_page_numbers_and_running_heads(void) {
    const char *pages[] = {
        "139\n\nContents\n\nAlpha text.\n\n140\n",
        "141\n\nContents\n\nBeta text.\n\n142\n",
        "143\n\nContents\n\nGamma text.\n\n144\n",
    };
    const uint32_t indices[] = { 0, 1, 2 };
    const document *doc = build_document(pages, indices, 3, "book");
    CHECK(emit(doc, false) > 0u);
    CHECK(!contains("139"));
    CHECK(!contains("140"));
    CHECK_U64_EQ(count_occurrences("Contents"), 1u);
    CHECK(contains("Alpha text."));
    CHECK(contains("Beta text."));
    CHECK(contains("Gamma text."));
}

static void test_typst_capacity_failure(void) {
    const char *pages[] = { "Hello world.\n" };
    const uint32_t indices[] = { 0 };
    const document *doc = build_document(pages, indices, 1, "book");
    typst_config config;
    config.geometry.page_width_pt10 = 4308u;
    config.geometry.page_height_pt10 = 6462u;
    config.geometry.margin_x_pt10 = 560u;
    config.geometry.margin_y_pt10 = 646u;
    config.geometry.body_pt10 = 105u;
    config.title = document_title(doc);
    config.title_length = doc->title_length;
    config.include_outline = false;
    char small[8];
    CHECK_U64_EQ(typst_emit(doc, &config, small, sizeof small), 0u);
}

static void test_typst_title_escaping(void) {
    const char *pages[] = { "Hello world.\n" };
    const uint32_t indices[] = { 0 };
    const document *doc = build_document(pages, indices, 1, "A \"quoted\" \\ title");
    CHECK(emit(doc, false) > 0u);
    CHECK(contains("#set document(title: \"A \\\"quoted\\\" \\\\ title\")"));
}

static void test_typst_empty_page(void) {
    const char *pages[] = { "" };
    const uint32_t indices[] = { 0 };
    const document *doc = build_document(pages, indices, 1, "book");
    uint64_t written = emit(doc, false);
    CHECK(written > 0u);
    CHECK(contains("#set page("));
}

static void test_typst_escapes_list_markers(void) {
    /* Typst reads these first characters as list, heading, and term markers,
     * so a paragraph that begins with one has to keep its literal meaning. */
    const char *pages[] = { "+ plus item\n\n= equals item\n\n/ slash item\n" };
    const uint32_t indices[] = { 0 };
    const document *doc = build_document(pages, indices, 1, "book");
    CHECK(emit(doc, false) > 0u);
    CHECK(contains("\\+ plus item"));
    CHECK(contains("\\= equals item"));
    CHECK(contains("\\/ slash item"));
}

static void test_typst_superscript_with_wrong_number(void) {
    /* The page has a footnote 1 and a reference to 2, so the reference must
     * stay a superscript and the orphan footnote must still be printed. */
    const char *pages[] = { "Body one.\n\nBody two.\n\nBody three.\n\nBody four with a reference\xC2\xB2 here.\n\n1 A note for another number.\n" };
    const uint32_t indices[] = { 0 };
    const document *doc = build_document(pages, indices, 1, "book");
    CHECK(emit(doc, false) > 0u);
    CHECK(contains("#super[2]"));
    CHECK(contains("#footnote[A note for another number.]"));
}

static void test_typst_repeated_superscript(void) {
    /* Footnote 1 is referenced twice, so only the first reference may carry
     * the note body; the second one stays a superscript. */
    const char *pages[] = { "Text\xC2\xB9 and more\xC2\xB9.\n\n1 A note.\n" };
    const uint32_t indices[] = { 0 };
    const document *doc = build_document(pages, indices, 1, "book");
    CHECK(emit(doc, false) > 0u);
    CHECK_U64_EQ(count_occurrences("#footnote["), 1u);
    CHECK(contains("#super[1]"));
}

static void test_typst_two_unmatched_footnotes(void) {
    /* Both notes sit in the tail of the page without a reference; they are
     * printed after the last block, separated by a space so typst keeps them
     * apart. */
    const char *pages[] = {
        "Body one.\n\nBody two.\n\nBody three.\n\nBody four.\n\nBody five.\n\nBody six.\n\n1 First note.\n\n2 Second note.\n",
    };
    const uint32_t indices[] = { 0 };
    const document *doc = build_document(pages, indices, 1, "book");
    CHECK(emit(doc, false) > 0u);
    CHECK_U64_EQ(count_occurrences("#footnote["), 2u);
    CHECK(contains("#footnote[First note.] #footnote[Second note.]"));
}

static void test_typst_footnote_only_page(void) {
    /* A page that holds nothing but a footnote still has to print it. */
    const char *pages[] = { "1 A note that fills the page\xC2\xB9.\n" };
    const uint32_t indices[] = { 0 };
    const document *doc = build_document(pages, indices, 1, "book");
    CHECK(emit(doc, false) > 0u);
    CHECK_U64_EQ(count_occurrences("#footnote["), 1u);
    CHECK(contains("A note that fills the page"));
}

static void test_typst_invalid_utf8_byte(void) {
    /* A byte that is not valid UTF-8 must be copied through instead of
     * stalling the superscript scanner. */
    const char *pages[] = { "abc\xFF def.\n" };
    const uint32_t indices[] = { 0 };
    const document *doc = build_document(pages, indices, 1, "book");
    CHECK(emit(doc, false) > 0u);
    CHECK(contains("abc\xFF def."));
}

static void test_typst_capacity_failure_is_atomic(void) {
    /* Every capacity below the required size has to fail as a whole: a
     * half-written document is never reported as success. */
    const char *pages[] = {
        "CHAPTER 1\n\nBody text long enough to need several writes.\n\n$$a + b$$\n",
        "CHAPTER 2\n\nMore body text.\n\n<table><tr><td>1</td></tr></table>\n",
    };
    const uint32_t indices[] = { 0, 1 };
    const document *doc = build_document(pages, indices, 2, NULL);
    typst_config config;
    config.geometry.page_width_pt10 = 4308u;
    config.geometry.page_height_pt10 = 6462u;
    config.geometry.margin_x_pt10 = 560u;
    config.geometry.margin_y_pt10 = 646u;
    config.geometry.body_pt10 = 105u;
    config.title = document_title(doc);
    config.title_length = doc->title_length;
    config.include_outline = false;
    char small[4096];
    uint64_t needed = typst_emit(doc, &config, small, sizeof small);
    CHECK(needed > 0u);
    CHECK(needed < sizeof small);
    for (uint64_t capacity = 1u; capacity < needed; capacity += 1u) {
        if (typst_emit(doc, &config, small, capacity) != 0u) {
            CHECK_DETAIL(false, "a partial document was reported as complete");
        }
    }
    CHECK_U64_EQ(typst_emit(doc, &config, small, needed), needed);
}

static void test_typst_empty_math_block(void) {
    /* A pair of dollar signs is a block the math path cannot emit; it must be
     * dropped without breaking the page. */
    const char *pages[] = { "$$\n\nBody text.\n" };
    const uint32_t indices[] = { 0 };
    const document *doc = build_document(pages, indices, 1, "book");
    CHECK(emit(doc, false) > 0u);
    CHECK(!contains("#align(center)"));
    CHECK(contains("Body text."));
}

void register_typst_emit_tests(void) {
    test_register("typst_header_and_paragraph", test_typst_header_and_paragraph);
    test_register("typst_escapes_markup", test_typst_escapes_markup);
    test_register("typst_chapters_and_outline", test_typst_chapters_and_outline);
    test_register("typst_no_outline_when_disabled", test_typst_no_outline_when_disabled);
    test_register("typst_footnotes", test_typst_footnotes);
    test_register("typst_superscript_without_footnote", test_typst_superscript_without_footnote);
    test_register("typst_unmatched_footnote_is_kept", test_typst_unmatched_footnote_is_kept);
    test_register("typst_math_and_table", test_typst_math_and_table);
    test_register("typst_skips_page_numbers_and_running_heads", test_typst_skips_page_numbers_and_running_heads);
    test_register("typst_capacity_failure", test_typst_capacity_failure);
    test_register("typst_title_escaping", test_typst_title_escaping);
    test_register("typst_empty_page", test_typst_empty_page);
    test_register("typst_escapes_list_markers", test_typst_escapes_list_markers);
    test_register("typst_superscript_with_wrong_number", test_typst_superscript_with_wrong_number);
    test_register("typst_repeated_superscript", test_typst_repeated_superscript);
    test_register("typst_two_unmatched_footnotes", test_typst_two_unmatched_footnotes);
    test_register("typst_footnote_only_page", test_typst_footnote_only_page);
    test_register("typst_invalid_utf8_byte", test_typst_invalid_utf8_byte);
    test_register("typst_capacity_failure_is_atomic", test_typst_capacity_failure_is_atomic);
    test_register("typst_empty_math_block", test_typst_empty_math_block);
}