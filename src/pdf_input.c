#include "pdf_input.h"

#include "asserts.h"

#include <fpdfview.h>

#include <stdio.h>

static bool pdf_library_initialized = false;

bool pdf_input_open(pdf_input *input, const char *pdf_path, char *error, uint64_t error_capacity) {
    ASSERT(input != NULL);
    ASSERT(pdf_path != NULL);
    ASSERT(error != NULL);
    ASSERT(error_capacity > 0);

    if (!pdf_library_initialized) {
        FPDF_InitLibrary();
        pdf_library_initialized = true;
    }
    input->library_initialized = true;

    FPDF_DOCUMENT document = FPDF_LoadDocument(pdf_path, NULL);
    if (document == NULL) {
        unsigned long code = FPDF_GetLastError();
        (void)snprintf(error, (size_t)error_capacity, "pdfium could not open %s (error %lu)", pdf_path, code);
        return false;
    }
    input->document = document;
    if (FPDF_GetPageCount((FPDF_DOCUMENT)input->document) == 0) {
        (void)snprintf(error, (size_t)error_capacity, "%s has no pages", pdf_path);
        FPDF_CloseDocument((FPDF_DOCUMENT)input->document);
        input->document = NULL;
        return false;
    }
    return true;
}

void pdf_input_close(pdf_input *input) {
    ASSERT(input != NULL);
    if (input->document != NULL) {
        FPDF_CloseDocument((FPDF_DOCUMENT)input->document);
        input->document = NULL;
    }
    if (pdf_library_initialized) {
        FPDF_DestroyLibrary();
        pdf_library_initialized = false;
    }
}

uint32_t pdf_input_page_count(const pdf_input *input) {
    ASSERT(input != NULL);
    ASSERT(input->document != NULL);
    return (uint32_t)FPDF_GetPageCount((FPDF_DOCUMENT)input->document);
}

bool pdf_input_page_size(const pdf_input *input, uint32_t page_index, uint32_t *width_pt10, uint32_t *height_pt10, char *error, uint64_t error_capacity) {
    ASSERT(input != NULL);
    ASSERT(width_pt10 != NULL);
    ASSERT(height_pt10 != NULL);

    FPDF_PAGE page = FPDF_LoadPage((FPDF_DOCUMENT)input->document, (int)page_index);
    if (page == NULL) {
        (void)snprintf(error, (size_t)error_capacity, "pdfium could not load page %u", page_index);
        return false;
    }
    float width = FPDF_GetPageWidthF(page);
    float height = FPDF_GetPageHeightF(page);
    FPDF_ClosePage(page);
    if (width <= 0.0f || height <= 0.0f) {
        (void)snprintf(error, (size_t)error_capacity, "page %u has no size", page_index);
        return false;
    }
    *width_pt10 = (uint32_t)(width * 10.0f + 0.5f);
    *height_pt10 = (uint32_t)(height * 10.0f + 0.5f);
    return true;
}

bool pdf_input_render_page(pdf_input *input, uint32_t page_index, uint32_t width, uint32_t height, uint8_t *bgra, uint64_t stride, char *error, uint64_t error_capacity) {
    ASSERT(input != NULL);
    ASSERT(bgra != NULL);
    ASSERT(width > 0);
    ASSERT(height > 0);
    ASSERT(stride >= (uint64_t)width * 4u);

    FPDF_PAGE page = FPDF_LoadPage((FPDF_DOCUMENT)input->document, (int)page_index);
    if (page == NULL) {
        (void)snprintf(error, (size_t)error_capacity, "pdfium could not load page %u", page_index);
        return false;
    }
    FPDF_BITMAP bitmap = FPDFBitmap_CreateEx((int)width, (int)height, FPDFBitmap_BGRA, bgra, (int)stride);
    if (bitmap == NULL) {
        FPDF_ClosePage(page);
        (void)snprintf(error, (size_t)error_capacity, "pdfium could not allocate a %ux%u bitmap", width, height);
        return false;
    }
    FPDFBitmap_FillRect(bitmap, 0, 0, (int)width, (int)height, 0xFFFFFFFFu);
    int rotation = FPDFPage_GetRotation(page) * 90;
    FPDF_RenderPageBitmap(bitmap, page, 0, 0, (int)width, (int)height, rotation, FPDF_ANNOT);
    FPDFBitmap_Destroy(bitmap);
    FPDF_ClosePage(page);
    return true;
}