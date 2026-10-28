#ifndef BETTER_PDF_DOCUMENT_H
#define BETTER_PDF_DOCUMENT_H

#include <stdbool.h>
#include <stdint.h>

/*
 * The document AST.
 *
 * Everything lives in one arena: the block array, the page array, the chapter
 * array, and a single text pool that every span points into. Nothing is
 * allocated after startup, and no pointer inside the AST is ever invalidated
 * because nothing moves.
 *
 * The OCR text of each page is normalized and classified as it arrives, then a
 * final pass finds running heads and chapter starts, where the whole book is
 * finally visible.
 */

typedef enum {
    BLOCK_PARAGRAPH = 0,
    BLOCK_HEADING = 1,
    BLOCK_FOOTNOTE = 2,
    BLOCK_PAGE_NUMBER = 3,
    BLOCK_TABLE = 4,
    BLOCK_MATH = 5,
    BLOCK_RUNNING_HEAD = 6
} block_kind;

#define DOCUMENT_BLOCKS_PER_PAGE 128u
#define DOCUMENT_MAX_PARAGRAPHS_PER_PAGE 512u
#define DOCUMENT_TEXT_MIN_BYTES (32u * 1024u * 1024u)
#define DOCUMENT_TEXT_BYTES_PER_PAGE (8u * 1024u)
#define DOCUMENT_NO_BLOCK UINT32_MAX

typedef struct {
    uint64_t text_offset;
    uint32_t text_length;
    uint16_t footnote_number;
    uint8_t kind;
    uint8_t level;
} block;

typedef struct {
    uint32_t first_block;
    uint32_t block_count;
    uint32_t source_page_index;
    uint32_t printed_page_number;
    uint8_t printed_is_roman;
    uint8_t has_printed_page_number;
} page;

typedef struct {
    uint32_t title_block;
    uint32_t first_page;
    uint32_t page_count;
    uint8_t level;
} chapter;

typedef struct {
    block *blocks;
    uint32_t block_count;
    uint32_t block_capacity;
    page *pages;
    uint32_t page_count;
    uint32_t page_capacity;
    chapter *chapters;
    uint32_t chapter_count;
    uint32_t chapter_capacity;
    char *text;
    uint64_t text_capacity;
    uint64_t text_used;
    uint32_t title_offset;
    uint32_t title_length;
    bool has_outline;
} document;

uint64_t document_text_capacity(uint32_t page_capacity);
uint64_t document_memory_required(uint32_t page_capacity, uint64_t text_capacity);
bool document_init(document *doc, void *memory, uint64_t memory_capacity, uint32_t page_capacity, uint64_t text_capacity);
bool document_add_page(document *doc, uint32_t source_page_index, const char *text, uint64_t length);
bool document_finalize(document *doc, const char *fallback_title, const char *title_override);
const char *document_block_text(const document *doc, const block *value);
const char *document_title(const document *doc);

#endif