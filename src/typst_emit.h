#ifndef BETTER_PDF_TYPST_EMIT_H
#define BETTER_PDF_TYPST_EMIT_H

#include "document.h"
#include "geometry.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * Turns the document AST into typst markup.
 *
 * The result is plain source text: a page rule sized from the input book, an
 * outline when the book has chapters, and one heading or paragraph after
 * another. Superscript footnote references are replaced with real typst
 * footnotes, which is why the output can renumber notes and keep them at the
 * bottom of the page they belong to.
 */

typedef struct {
    page_geometry geometry;
    const char *title;
    uint64_t title_length;
    bool include_outline;
} typst_config;

uint64_t typst_emit(const document *doc, const typst_config *config, char *output, uint64_t capacity);

#endif