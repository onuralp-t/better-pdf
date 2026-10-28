#include "typst_emit.h"

#include "asserts.h"
#include "ocr_text.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char *output;
    uint64_t capacity;
    uint64_t used;
    bool failed;
} emitter;

static void typst_emit_raw(emitter *target, const char *text, uint64_t length) {
    ASSERT(target != NULL);
    ASSERT(text != NULL);
    if (target->failed) {
        return;
    }
    if (target->used + length > target->capacity) {
        target->failed = true;
        return;
    }
    memcpy(target->output + target->used, text, (size_t)length);
    target->used += length;
}

static void typst_emit_literal(emitter *target, const char *text) {
    ASSERT(target != NULL);
    ASSERT(text != NULL);
    typst_emit_raw(target, text, (uint64_t)strlen(text));
}

static void typst_emit_format(emitter *target, const char *format, ...) {
    ASSERT(target != NULL);
    /* Every format string on a call site is short and takes at most two
     * uint32 values, so the longest result is well under this buffer. */
    char buffer[64];
    va_list arguments;
    va_start(arguments, format);
    uint64_t written = (uint64_t)vsnprintf(buffer, sizeof buffer, format, arguments);
    va_end(arguments);
    ASSERT(written < (uint64_t)sizeof buffer);
    typst_emit_raw(target, buffer, (uint64_t)written);
}

static void typst_emit_pt10(emitter *target, uint32_t value) {
    ASSERT(target != NULL);
    typst_emit_format(target, "%u.%upt", value / 10u, value % 10u);
}

static bool typst_escape_character(char character) {
    switch (character) {
        case '\\':
        case '#':
        case '$':
        case '*':
        case '_':
        case '`':
        case '<':
        case '>':
        case '@':
        case '~':
        case '[':
        case ']':
            return true;
        default:
            return false;
    }
}

static void typst_emit_escaped(emitter *target, const char *text, uint64_t length) {
    ASSERT(target != NULL);
    ASSERT(text != NULL);
    for (uint64_t index = 0; index < length; index += 1) {
        char character = text[index];
        if (typst_escape_character(character)) {
            typst_emit_raw(target, "\\", 1u);
        }
        typst_emit_raw(target, &character, 1u);
    }
}

static void typst_emit_string_literal(emitter *target, const char *text, uint64_t length) {
    ASSERT(target != NULL);
    ASSERT(text != NULL);
    for (uint64_t index = 0; index < length; index += 1) {
        char character = text[index];
        if (character == '\\' || character == '"') {
            typst_emit_raw(target, "\\", 1u);
        }
        typst_emit_raw(target, &character, 1u);
    }
}

static bool typst_leading_marker(char character) {
    return character == '-' || character == '+' || character == '=' || character == '/';
}

static uint32_t typst_find_footnote(const document *doc, const page *current, uint32_t number, bool *used) {
    ASSERT(doc != NULL);
    ASSERT(current != NULL);
    ASSERT(used != NULL);
    for (uint32_t offset = 0; offset < current->block_count; offset += 1) {
        if (used[offset]) {
            continue;
        }
        const block *candidate = &doc->blocks[current->first_block + offset];
        if (candidate->kind == BLOCK_FOOTNOTE && candidate->footnote_number == number) {
            used[offset] = true;
            return current->first_block + offset;
        }
    }
    return DOCUMENT_NO_BLOCK;
}

static void typst_emit_footnote_body(emitter *target, const document *doc, const block *value) {
    ASSERT(target != NULL);
    ASSERT(doc != NULL);
    ASSERT(value != NULL);
    const char *text = document_block_text(doc, value);
    uint64_t length = value->text_length;
    uint64_t begin = ocr_strip_footnote_marker(text, length);
    typst_emit_escaped(target, text + begin, length - begin);
}

static void typst_emit_inline(emitter *target, const document *doc, const page *current, const block *value, bool *footnote_used) {
    ASSERT(target != NULL);
    ASSERT(doc != NULL);
    ASSERT(current != NULL);
    ASSERT(value != NULL);
    ASSERT(footnote_used != NULL);

    const char *text = document_block_text(doc, value);
    uint64_t length = value->text_length;
    /* The document builder never stores an empty block: an empty paragraph is
     * dropped before it becomes one. */
    ASSERT(length > 0u);
    if (typst_leading_marker(text[0])) {
        typst_emit_raw(target, "\\", 1u);
    }

    uint64_t begin = 0;
    uint64_t position = 0;
    while (position < length) {
        char digits[16];
        uint64_t run = ocr_superscript_run(text, length, position, digits, sizeof digits - 1u);
        if (run > 0u) {
            typst_emit_escaped(target, text + begin, position - begin);
            uint32_t number = 0;
            for (uint32_t index = 0; digits[index] != '\0'; index += 1) {
                number = number * 10u + (uint32_t)(digits[index] - '0');
            }
            uint32_t footnote = typst_find_footnote(doc, current, number, footnote_used);
            if (footnote != DOCUMENT_NO_BLOCK) {
                typst_emit_literal(target, "#footnote[");
                typst_emit_footnote_body(target, doc, &doc->blocks[footnote]);
                typst_emit_literal(target, "]");
            } else {
                typst_emit_literal(target, "#super[");
                typst_emit_raw(target, digits, (uint64_t)strlen(digits));
                typst_emit_literal(target, "]");
            }
            position += run;
            begin = position;
        } else {
            uint32_t code_point = 0;
            uint64_t size = ocr_utf8_decode(text, length, position, &code_point);
            position += size == 0u ? 1u : size;
        }
    }
    typst_emit_escaped(target, text + begin, length - begin);
}

static void typst_emit_block(emitter *target, const document *doc, const page *current, const block *value, bool *footnote_used) {
    ASSERT(target != NULL);
    ASSERT(value != NULL);

    block_kind kind = (block_kind)value->kind;
    if (kind == BLOCK_HEADING) {
        /* Only the classifier creates headings, and it always assigns a level. */
        ASSERT(value->level >= 1u);
        typst_emit_format(target, "#heading(level: %u)[", (uint32_t)value->level);
        typst_emit_inline(target, doc, current, value, footnote_used);
        typst_emit_literal(target, "]");
        return;
    }
    if (kind == BLOCK_MATH) {
        const char *text = document_block_text(doc, value);
        uint64_t length = value->text_length;
        /* Only the classifier creates math blocks, and it requires the four
         * dollar signs that are stripped here. */
        ASSERT(length >= 4u);
        typst_emit_literal(target, "#align(center)[$ ");
        typst_emit_raw(target, text + 2u, length - 4u);
        typst_emit_literal(target, " $]");
        return;
    }
    typst_emit_inline(target, doc, current, value, footnote_used);
}

static bool typst_block_emitted(block_kind kind) {
    return kind == BLOCK_HEADING || kind == BLOCK_PARAGRAPH || kind == BLOCK_TABLE || kind == BLOCK_MATH;
}

static bool typst_emit_unmatched_footnotes(emitter *target, const document *doc, const page *current, bool *footnote_used) {
    ASSERT(target != NULL);
    ASSERT(current != NULL);
    bool emitted = false;
    for (uint32_t offset = 0; offset < current->block_count; offset += 1) {
        const block *candidate = &doc->blocks[current->first_block + offset];
        if (candidate->kind != BLOCK_FOOTNOTE || footnote_used[offset]) {
            continue;
        }
        footnote_used[offset] = true;
        if (emitted) {
            typst_emit_literal(target, " ");
        }
        typst_emit_literal(target, "#footnote[");
        typst_emit_footnote_body(target, doc, candidate);
        typst_emit_literal(target, "]");
        emitted = true;
    }
    return emitted;
}

static void typst_emit_page(emitter *target, const document *doc, uint32_t page_index) {
    ASSERT(target != NULL);
    ASSERT(page_index < doc->page_count);

    const page *current = &doc->pages[page_index];
    bool footnote_used[DOCUMENT_BLOCKS_PER_PAGE];
    memset(footnote_used, 0, sizeof footnote_used);

    uint32_t last_emitted = UINT32_MAX;
    for (uint32_t offset = 0; offset < current->block_count; offset += 1) {
        const block *candidate = &doc->blocks[current->first_block + offset];
        if (typst_block_emitted((block_kind)candidate->kind)) {
            last_emitted = offset;
        }
    }

    for (uint32_t offset = 0; offset < current->block_count; offset += 1) {
        const block *candidate = &doc->blocks[current->first_block + offset];
        block_kind kind = (block_kind)candidate->kind;
        if (!typst_block_emitted(kind)) {
            continue;
        }
        typst_emit_block(target, doc, current, candidate, footnote_used);
        if (offset == last_emitted) {
            (void)typst_emit_unmatched_footnotes(target, doc, current, footnote_used);
        }
        typst_emit_literal(target, "\n\n");
    }
    if (last_emitted == UINT32_MAX) {
        if (typst_emit_unmatched_footnotes(target, doc, current, footnote_used)) {
            typst_emit_literal(target, "\n\n");
        }
    }
}

uint64_t typst_emit(const document *doc, const typst_config *config, char *output, uint64_t capacity) {
    ASSERT(doc != NULL);
    ASSERT(config != NULL);
    ASSERT(output != NULL);
    ASSERT(capacity > 0);

    emitter target;
    target.output = output;
    target.capacity = capacity;
    target.used = 0;
    target.failed = false;

    typst_emit_literal(&target, "#set document(title: \"");
    typst_emit_string_literal(&target, config->title, config->title_length);
    typst_emit_literal(&target, "\")\n");

    typst_emit_literal(&target, "#set page(width: ");
    typst_emit_pt10(&target, config->geometry.page_width_pt10);
    typst_emit_literal(&target, ", height: ");
    typst_emit_pt10(&target, config->geometry.page_height_pt10);
    typst_emit_literal(&target, ", margin: (x: ");
    typst_emit_pt10(&target, config->geometry.margin_x_pt10);
    typst_emit_literal(&target, ", y: ");
    typst_emit_pt10(&target, config->geometry.margin_y_pt10);
    typst_emit_literal(&target, "), numbering: \"1\")\n");

    typst_emit_literal(&target, "#set text(font: \"Libertinus Serif\", size: ");
    typst_emit_pt10(&target, config->geometry.body_pt10);
    typst_emit_literal(&target, ")\n");
    typst_emit_literal(&target, "#set par(justify: true, leading: 0.62em, spacing: 1.05em)\n\n");

    if (config->include_outline) {
        typst_emit_literal(&target, "#outline(title: [Contents], indent: 1.2em)\n");
        typst_emit_literal(&target, "#pagebreak(weak: true)\n\n");
    }

    for (uint32_t page_index = 0; page_index < doc->page_count; page_index += 1) {
        for (uint32_t chapter_index = 1; chapter_index < doc->chapter_count; chapter_index += 1) {
            if (doc->chapters[chapter_index].first_page == page_index) {
                typst_emit_literal(&target, "#pagebreak(weak: true)\n\n");
                break;
            }
        }
        typst_emit_page(&target, doc, page_index);
    }

    if (target.failed) {
        return 0;
    }
    return target.used;
}