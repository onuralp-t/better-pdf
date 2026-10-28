#ifndef BETTER_PDF_PDF_INPUT_H
#define BETTER_PDF_PDF_INPUT_H

#include <stdbool.h>
#include <stdint.h>

/*
 * pdfium glue.
 *
 * Pages are rendered to a BGRA bitmap at an explicit pixel size, which is how
 * the pipeline controls the vision encoder's input exactly. pdfium is linked
 * against the vendored import library; pdfium.dll has to sit next to the
 * executable.
 */

typedef struct {
    void *document;
    bool library_initialized;
} pdf_input;

bool pdf_input_open(pdf_input *input, const char *pdf_path, char *error, uint64_t error_capacity);
void pdf_input_close(pdf_input *input);
uint32_t pdf_input_page_count(const pdf_input *input);
bool pdf_input_page_size(const pdf_input *input, uint32_t page_index, uint32_t *width_pt10, uint32_t *height_pt10, char *error, uint64_t error_capacity);
bool pdf_input_render_page(pdf_input *input, uint32_t page_index, uint32_t width, uint32_t height, uint8_t *bgra, uint64_t stride, char *error, uint64_t error_capacity);

#endif