#include "document.h"

#include "arena.h"
#include "asserts.h"
#include "ocr_text.h"

#include <string.h>

#define DOCUMENT_ALIGNMENT 8u
#define DOCUMENT_TITLE_CAPACITY 512u
#define DOCUMENT_RUNNING_HEAD_CAPACITY 256u

typedef struct {
    uint64_t text_offset;
    uint32_t text_length;
    uint32_t marker;
    bool has_marker;
} paragraph_record;

typedef struct {
    paragraph_record records[DOCUMENT_MAX_PARAGRAPHS_PER_PAGE];
    uint32_t count;
} paragraph_list;

uint64_t document_text_capacity(uint32_t page_capacity) {
    ASSERT(page_capacity > 0);
    uint64_t scaled = (uint64_t)page_capacity * (uint64_t)DOCUMENT_TEXT_BYTES_PER_PAGE;
    return scaled > (uint64_t)DOCUMENT_TEXT_MIN_BYTES ? scaled : (uint64_t)DOCUMENT_TEXT_MIN_BYTES;
}

uint64_t document_memory_required(uint32_t page_capacity, uint64_t text_capacity) {
    ASSERT(page_capacity > 0);
    ASSERT(text_capacity > 0);
    uint64_t block_capacity = (uint64_t)page_capacity * (uint64_t)DOCUMENT_BLOCKS_PER_PAGE;
    uint64_t total = block_capacity * (uint64_t)sizeof(block);
    total += (uint64_t)page_capacity * (uint64_t)sizeof(page);
    total += ((uint64_t)page_capacity + 1u) * (uint64_t)sizeof(chapter);
    total += text_capacity;
    total += 128u;
    return total;
}

bool document_init(document *doc, void *memory, uint64_t memory_capacity, uint32_t page_capacity, uint64_t text_capacity) {
    ASSERT(doc != NULL);
    ASSERT(memory != NULL);
    ASSERT(page_capacity > 0);
    ASSERT(text_capacity > 0);
    ASSERT(memory_capacity >= document_memory_required(page_capacity, text_capacity));

    arena storage;
    arena_init(&storage, memory, memory_capacity);

    uint64_t block_capacity = (uint64_t)page_capacity * (uint64_t)DOCUMENT_BLOCKS_PER_PAGE;
    doc->blocks = (block *)arena_alloc(&storage, block_capacity * (uint64_t)sizeof(block), DOCUMENT_ALIGNMENT);
    doc->block_count = 0;
    doc->block_capacity = (uint32_t)block_capacity;
    doc->pages = (page *)arena_alloc(&storage, (uint64_t)page_capacity * (uint64_t)sizeof(page), DOCUMENT_ALIGNMENT);
    doc->page_count = 0;
    doc->page_capacity = page_capacity;
    doc->chapters = (chapter *)arena_alloc(&storage, ((uint64_t)page_capacity + 1u) * (uint64_t)sizeof(chapter), DOCUMENT_ALIGNMENT);
    doc->chapter_count = 0;
    doc->chapter_capacity = page_capacity + 1u;
    doc->text = (char *)arena_alloc(&storage, text_capacity, 1u);
    doc->text_capacity = text_capacity;
    doc->text_used = 0;
    doc->title_offset = 0;
    doc->title_length = 0;
    doc->has_outline = false;
    return true;
}

const char *document_block_text(const document *doc, const block *value) {
    ASSERT(doc != NULL);
    ASSERT(value != NULL);
    ASSERT(value->text_offset <= doc->text_used);
    ASSERT(value->text_offset + value->text_length <= doc->text_used);
    return doc->text + value->text_offset;
}

const char *document_title(const document *doc) {
    ASSERT(doc != NULL);
    return doc->text + doc->title_offset;
}

static uint32_t document_first_content_block(const document *doc, uint32_t page_index) {
    ASSERT(page_index < doc->page_count);
    const page *value = &doc->pages[page_index];
    for (uint32_t offset = 0; offset < value->block_count; offset += 1) {
        uint32_t index = value->first_block + offset;
        block_kind kind = (block_kind)doc->blocks[index].kind;
        if (kind != BLOCK_PAGE_NUMBER && kind != BLOCK_RUNNING_HEAD) {
            return index;
        }
    }
    return DOCUMENT_NO_BLOCK;
}

static bool document_push_block(document *doc, block_kind kind, uint8_t level, uint16_t footnote_number, uint64_t text_offset, uint32_t text_length) {
    if (doc->block_count >= doc->block_capacity) {
        return false;
    }
    block *value = &doc->blocks[doc->block_count];
    value->text_offset = text_offset;
    value->text_length = text_length;
    value->footnote_number = footnote_number;
    value->kind = (uint8_t)kind;
    value->level = level;
    doc->block_count += 1;
    return true;
}

static bool document_gather_paragraphs(document *doc, const char *text, uint64_t length, paragraph_list *paragraphs) {
    ASSERT(doc != NULL);
    ASSERT(text != NULL);
    ASSERT(paragraphs != NULL);

    paragraphs->count = 0;
    uint64_t cursor = 0;
    uint64_t begin = 0;
    uint64_t end = 0;
    while (ocr_next_paragraph(text, length, cursor, &begin, &end)) {
        uint64_t written = 0;
        uint64_t remaining = doc->text_capacity - doc->text_used;
        if (!ocr_normalize_span(text, begin, end, doc->text + doc->text_used, remaining, &written)) {
            return false;
        }
        cursor = end;
        if (written == 0u) {
            continue;
        }
        if (paragraphs->count >= DOCUMENT_MAX_PARAGRAPHS_PER_PAGE) {
            return false;
        }
        paragraph_record *record = &paragraphs->records[paragraphs->count];
        record->text_offset = doc->text_used;
        record->text_length = (uint32_t)written;
        record->marker = 0;
        record->has_marker = false;
        doc->text_used += written;
        paragraphs->count += 1;
    }
    return true;
}

static bool document_next_starts_lowercase(const document *doc, const paragraph_list *paragraphs, uint32_t index) {
    if (index + 1u >= paragraphs->count) {
        return false;
    }
    const paragraph_record *next = &paragraphs->records[index + 1u];
    if (next->text_length == 0u) {
        return false;
    }
    char first = doc->text[next->text_offset];
    return first >= 'a' && first <= 'z';
}

static bool document_add_paragraphs(document *doc, const paragraph_list *paragraphs, uint64_t page_text_offset, uint64_t page_text_length, bool *has_printed_number, uint32_t *printed_number, bool *printed_is_roman) {
    ASSERT(doc != NULL);
    ASSERT(paragraphs != NULL);

    bool content_seen = false;
    uint32_t index = 0;
    while (index < paragraphs->count) {
        const paragraph_record *record = &paragraphs->records[index];
        const char *text = doc->text + record->text_offset;
        uint64_t length = (uint64_t)record->text_length;

        if (index == 0u || index + 1u == paragraphs->count) {
            ocr_number_result number;
            if (ocr_number_token(text, length, &number)) {
                if (!document_push_block(doc, BLOCK_PAGE_NUMBER, 0, 0, record->text_offset, record->text_length)) {
                    return false;
                }
                if (!*has_printed_number) {
                    *has_printed_number = true;
                    *printed_number = number.number;
                    *printed_is_roman = number.roman;
                }
                index += 1u;
                continue;
            }
        }

        uint32_t marker = 0;
        uint64_t body_offset = 0;
        bool footnote_candidate = ocr_footnote_marker(text, length, &marker, &body_offset);
        if (footnote_candidate) {
            bool in_tail = (uint64_t)index * 10u >= (uint64_t)paragraphs->count * 7u;
            if (in_tail || ocr_superscript_present(doc->text + page_text_offset, page_text_length, marker)) {
                if (!document_push_block(doc, BLOCK_FOOTNOTE, 0, (uint16_t)marker, record->text_offset, record->text_length)) {
                    return false;
                }
                content_seen = true;
                index += 1u;
                continue;
            }
        }

        block_kind explicit_kind = BLOCK_PARAGRAPH;
        if (ocr_detect_table(text, length)) {
            explicit_kind = BLOCK_TABLE;
        } else if (ocr_detect_math(text, length)) {
            explicit_kind = BLOCK_MATH;
        }
        if (explicit_kind != BLOCK_PARAGRAPH) {
            if (!document_push_block(doc, explicit_kind, 0, 0, record->text_offset, record->text_length)) {
                return false;
            }
            content_seen = true;
            index += 1u;
            continue;
        }

        ocr_heading_result heading;
        bool next_lowercase = document_next_starts_lowercase(doc, paragraphs, index);
        ocr_classify_heading(text, length, !content_seen, next_lowercase, &heading);
        if (heading.is_heading) {
            uint64_t heading_offset = record->text_offset;
            uint32_t heading_length = record->text_length;
            bool merged = false;
            if (index + 1u < paragraphs->count) {
                const paragraph_record *subtitle = &paragraphs->records[index + 1u];
                if (ocr_is_parenthetical(doc->text + subtitle->text_offset, (uint64_t)subtitle->text_length)) {
                    uint64_t total = (uint64_t)heading_length + 1u + (uint64_t)subtitle->text_length;
                    if (doc->text_used + total > doc->text_capacity) {
                        return false;
                    }
                    char *combined = doc->text + doc->text_used;
                    memcpy(combined, doc->text + heading_offset, (size_t)heading_length);
                    combined[heading_length] = ' ';
                    memcpy(combined + heading_length + 1u, doc->text + subtitle->text_offset, (size_t)subtitle->text_length);
                    heading_offset = doc->text_used;
                    heading_length = (uint32_t)total;
                    doc->text_used += total;
                    merged = true;
                }
            }
            if (!document_push_block(doc, BLOCK_HEADING, heading.level, 0, heading_offset, heading_length)) {
                return false;
            }
            content_seen = true;
            index += merged ? 2u : 1u;
            continue;
        }

        if (!document_push_block(doc, BLOCK_PARAGRAPH, 0, 0, record->text_offset, record->text_length)) {
            return false;
        }
        content_seen = true;
        index += 1u;
    }
    return true;
}

bool document_add_page(document *doc, uint32_t source_page_index, const char *text, uint64_t length) {
    ASSERT(doc != NULL);
    ASSERT(text != NULL);

    if (doc->page_count >= doc->page_capacity) {
        return false;
    }

    uint64_t page_text_offset = doc->text_used;
    paragraph_list paragraphs;
    if (!document_gather_paragraphs(doc, text, length, &paragraphs)) {
        return false;
    }

    page *current = &doc->pages[doc->page_count];
    uint32_t first_block = doc->block_count;
    bool has_printed_number = false;
    uint32_t printed_number = 0;
    bool printed_is_roman = false;
    if (!document_add_paragraphs(doc, &paragraphs, page_text_offset, doc->text_used - page_text_offset, &has_printed_number, &printed_number, &printed_is_roman)) {
        return false;
    }

    current->first_block = first_block;
    current->block_count = doc->block_count - first_block;
    current->source_page_index = source_page_index;
    current->printed_page_number = printed_number;
    current->printed_is_roman = printed_is_roman ? 1u : 0u;
    current->has_printed_page_number = has_printed_number ? 1u : 0u;
    doc->page_count += 1;
    return true;
}

static bool document_same_normalized(const document *doc, const block *first, const block *second) {
    char first_text[DOCUMENT_RUNNING_HEAD_CAPACITY];
    char second_text[DOCUMENT_RUNNING_HEAD_CAPACITY];
    uint64_t first_length = ocr_normalize(document_block_text(doc, first), first->text_length, first_text, sizeof first_text);
    uint64_t second_length = ocr_normalize(document_block_text(doc, second), second->text_length, second_text, sizeof second_text);
    if (first_length == 0u || second_length == 0u || first_length != second_length) {
        return false;
    }
    return memcmp(first_text, second_text, (size_t)first_length) == 0;
}

static void document_demote_duplicate_level_one(document *doc) {
    ASSERT(doc != NULL);
    bool has_previous = false;
    uint32_t previous_page = 0;
    for (uint32_t page_index = 0; page_index < doc->page_count; page_index += 1) {
        const page *value = &doc->pages[page_index];
        for (uint32_t offset = 0; offset < value->block_count; offset += 1) {
            block *candidate = &doc->blocks[value->first_block + offset];
            if (candidate->kind == BLOCK_HEADING && candidate->level == 1u) {
                if (has_previous && previous_page == page_index) {
                    candidate->level = 2u;
                } else {
                    has_previous = true;
                    previous_page = page_index;
                }
            }
        }
    }
}

static void document_mark_running_heads(document *doc) {
    ASSERT(doc != NULL);
    for (uint32_t page_index = 0; page_index < doc->page_count; page_index += 1) {
        uint32_t first = document_first_content_block(doc, page_index);
        if (first == DOCUMENT_NO_BLOCK) {
            continue;
        }
        block *candidate = &doc->blocks[first];
        if (candidate->kind != BLOCK_PARAGRAPH && candidate->kind != BLOCK_HEADING) {
            continue;
        }
        uint32_t matches = 1;
        for (uint32_t other_page = page_index + 1u; other_page < doc->page_count; other_page += 1) {
            uint32_t other = document_first_content_block(doc, other_page);
            if (other == DOCUMENT_NO_BLOCK) {
                continue;
            }
            if (document_same_normalized(doc, candidate, &doc->blocks[other])) {
                matches += 1u;
            }
        }
        if (matches < 3u) {
            continue;
        }
        for (uint32_t other_page = page_index + 1u; other_page < doc->page_count; other_page += 1) {
            uint32_t other = document_first_content_block(doc, other_page);
            if (other == DOCUMENT_NO_BLOCK) {
                continue;
            }
            if (document_same_normalized(doc, candidate, &doc->blocks[other])) {
                doc->blocks[other].kind = BLOCK_RUNNING_HEAD;
            }
        }
    }
}

static void document_build_chapters(document *doc) {
    ASSERT(doc != NULL);
    doc->chapter_count = 0;
    for (uint32_t page_index = 0; page_index < doc->page_count; page_index += 1) {
        const page *value = &doc->pages[page_index];
        for (uint32_t offset = 0; offset < value->block_count; offset += 1) {
            uint32_t index = value->first_block + offset;
            if (doc->blocks[index].kind == BLOCK_HEADING && doc->blocks[index].level == 1u) {
                ASSERT(doc->chapter_count < doc->chapter_capacity);
                chapter *current = &doc->chapters[doc->chapter_count];
                current->title_block = index;
                current->first_page = page_index;
                current->page_count = 0;
                current->level = 1;
                doc->chapter_count += 1;
            }
        }
    }
    if (doc->chapter_count == 0u) {
        chapter *current = &doc->chapters[0];
        current->title_block = DOCUMENT_NO_BLOCK;
        current->first_page = 0;
        current->page_count = doc->page_count;
        current->level = 1;
        doc->chapter_count = 1;
        return;
    }
    for (uint32_t index = 0; index < doc->chapter_count; index += 1) {
        uint32_t next_page = index + 1u < doc->chapter_count ? doc->chapters[index + 1u].first_page : doc->page_count;
        doc->chapters[index].page_count = next_page - doc->chapters[index].first_page;
    }
}

static bool document_set_title(document *doc, const char *fallback_title, const char *title_override) {
    ASSERT(doc != NULL);
    ASSERT(fallback_title != NULL);

    char buffer[DOCUMENT_TITLE_CAPACITY];
    uint64_t length = 0;
    if (title_override != NULL) {
        length = (uint64_t)strlen(title_override);
        if (length >= sizeof buffer) {
            length = sizeof buffer - 1u;
        }
        memcpy(buffer, title_override, (size_t)length);
    } else if (doc->chapters[0].title_block != DOCUMENT_NO_BLOCK) {
        const block *value = &doc->blocks[doc->chapters[0].title_block];
        length = value->text_length;
        if (length >= sizeof buffer) {
            length = sizeof buffer - 1u;
        }
        memcpy(buffer, document_block_text(doc, value), (size_t)length);
    } else {
        length = (uint64_t)strlen(fallback_title);
        if (length >= sizeof buffer) {
            length = sizeof buffer - 1u;
        }
        memcpy(buffer, fallback_title, (size_t)length);
    }
    doc->title_offset = (uint32_t)doc->text_used;
    doc->title_length = (uint32_t)length;
    if (doc->text_used + length + 1u > doc->text_capacity) {
        return false;
    }
    memcpy(doc->text + doc->text_used, buffer, (size_t)length);
    doc->text[doc->text_used + length] = '\0';
    doc->text_used += length + 1u;
    return true;
}

bool document_finalize(document *doc, const char *fallback_title, const char *title_override) {
    ASSERT(doc != NULL);
    ASSERT(fallback_title != NULL);
    if (doc->page_count == 0u) {
        return false;
    }
    document_demote_duplicate_level_one(doc);
    document_mark_running_heads(doc);
    document_build_chapters(doc);
    if (!document_set_title(doc, fallback_title, title_override)) {
        return false;
    }
    doc->has_outline = doc->chapter_count >= 2u;
    return true;
}