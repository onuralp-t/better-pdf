#include "test.h"

#include "json.h"
#include "pipeline.h"
#include "platform_win32.h"

#include <stdio.h>
#include <string.h>

static void test_pipeline_select_range(void) {
    uint32_t first = 0;
    uint32_t last = 0;
    CHECK(pipeline_select_range(543u, 1u, 0u, &first, &last));
    CHECK_U64_EQ(first, 0);
    CHECK_U64_EQ(last, 542);
    CHECK(pipeline_select_range(543u, 4u, 4u, &first, &last));
    CHECK_U64_EQ(first, 3);
    CHECK_U64_EQ(last, 3);
    CHECK(pipeline_select_range(543u, 300u, 543u, &first, &last));
    CHECK_U64_EQ(first, 299);
    CHECK_U64_EQ(last, 542);

    CHECK(!pipeline_select_range(543u, 0u, 0u, &first, &last));
    CHECK(!pipeline_select_range(543u, 544u, 0u, &first, &last));
    CHECK(!pipeline_select_range(543u, 1u, 544u, &first, &last));
    CHECK(!pipeline_select_range(543u, 5u, 4u, &first, &last));
    CHECK(!pipeline_select_range(0u, 1u, 0u, &first, &last));
}

static void test_pipeline_work_directory(void) {
    char buffer[64];
    CHECK(pipeline_work_directory("out.pdf", NULL, buffer, sizeof buffer));
    CHECK_STR_EQ(buffer, "out.pdf.work");
    CHECK(pipeline_work_directory("out.pdf", "work", buffer, sizeof buffer));
    CHECK_STR_EQ(buffer, "work");
    CHECK(!pipeline_work_directory("out.pdf", "work", buffer, 4));
    CHECK(!pipeline_work_directory("a very long output name that does not fit.pdf", NULL, buffer, 8));
}

static void test_pipeline_file_stem(void) {
    char buffer[64];
    CHECK(pipeline_file_stem("samples\\clean.pdf", buffer, sizeof buffer));
    CHECK_STR_EQ(buffer, "clean");
    CHECK(pipeline_file_stem("C:/books/big.book.PDF", buffer, sizeof buffer));
    CHECK_STR_EQ(buffer, "big.book");
    CHECK(pipeline_file_stem("plain", buffer, sizeof buffer));
    CHECK_STR_EQ(buffer, "plain");
    CHECK(pipeline_file_stem("noext.txt", buffer, sizeof buffer));
    CHECK_STR_EQ(buffer, "noext.txt");
    CHECK(pipeline_file_stem("\\", buffer, sizeof buffer));
    CHECK_STR_EQ(buffer, "document");
    CHECK(pipeline_file_stem("", buffer, sizeof buffer));
    CHECK_STR_EQ(buffer, "document");
    CHECK(!pipeline_file_stem("clean.pdf", buffer, 4));
}

static void test_pipeline_plan_sizes(void) {
    pipeline_sizes sizes;
    CHECK(pipeline_plan_sizes(543u, 1003520u, &sizes));
    CHECK(sizes.document_bytes >= (uint64_t)543u * 8u);
    CHECK(sizes.raster_bytes == 1003520u * 4u);
    CHECK(sizes.png_bytes >= sizes.raster_bytes);
    CHECK(sizes.base64_bytes > sizes.png_bytes);
    CHECK(sizes.request_bytes > sizes.base64_bytes);
    CHECK(sizes.total_bytes < PIPELINE_MEMORY_LIMIT_BYTES);
    CHECK(sizes.total_bytes > sizes.typst_bytes);

    CHECK(!pipeline_plan_sizes(0u, 1003520u, &sizes));
    CHECK(!pipeline_plan_sizes(543u, 0u, &sizes));
}

static void test_pipeline_plan_sizes_rejects_absurd_budget(void) {
    pipeline_sizes sizes;
    CHECK(!pipeline_plan_sizes(100000u, 2000000000u, &sizes));
}

static void test_pipeline_request_build(void) {
    static char request[4096];
    uint64_t length = pipeline_request_build("QUJD", 4u, request, sizeof request);
    CHECK(length > 0u);
    CHECK_U64_EQ(length, (uint64_t)strlen(request));

    json_value root;
    CHECK(json_parse_document(request, length, &root));
    CHECK_U64_EQ(root.kind, JSON_KIND_OBJECT);

    json_value messages;
    CHECK(json_object_get(request, length, root, "messages", &messages));
    CHECK_U64_EQ(messages.kind, JSON_KIND_ARRAY);
    json_value message;
    CHECK(json_array_get(request, length, messages, 0u, &message));
    json_value content;
    CHECK(json_object_get(request, length, message, "content", &content));
    CHECK_U64_EQ(content.kind, JSON_KIND_ARRAY);

    json_value image;
    CHECK(json_array_get(request, length, content, 0u, &image));
    json_value image_url;
    CHECK(json_object_get(request, length, image, "image_url", &image_url));
    json_value url;
    CHECK(json_object_get(request, length, image_url, "url", &url));
    CHECK_U64_EQ(url.kind, JSON_KIND_STRING);

    char url_buffer[64];
    uint64_t url_length = 0;
    CHECK(json_string_copy(request, length, url, url_buffer, sizeof url_buffer, &url_length));
    CHECK_U64_EQ(url_length, 26u);
    CHECK(memcmp(url_buffer, "data:image/png;base64,QUJD", 26u) == 0);

    json_value text_item;
    CHECK(json_array_get(request, length, content, 1u, &text_item));
    json_value text;
    CHECK(json_object_get(request, length, text_item, "text", &text));
    char text_buffer[16];
    uint64_t text_length = 0;
    CHECK(json_string_copy(request, length, text, text_buffer, sizeof text_buffer, &text_length));
    CHECK_U64_EQ(text_length, 4u);
    CHECK(memcmp(text_buffer, "OCR:", 4u) == 0);

    CHECK_U64_EQ(pipeline_request_build("QUJD", 4u, request, 32), 0u);
    CHECK(pipeline_request_build("", 0u, request, sizeof request) > 0u);
}


static void pipeline_write_u32(FILE *file, uint32_t value) {
    (void)fwrite(&value, sizeof value, 1, file);
}

static void pipeline_write_u64(FILE *file, uint64_t value) {
    (void)fwrite(&value, sizeof value, 1, file);
}

static void pipeline_write_string(FILE *file, const char *text) {
    pipeline_write_u64(file, (uint64_t)strlen(text));
    (void)fwrite(text, 1, strlen(text), file);
}

static bool pipeline_build_mmproj(const char *directory, uint32_t type, uint64_t value) {
    char path[512];
    (void)snprintf(path, sizeof path, "%s\\PaddleOCR-VL-1.6-GGUF\\PaddleOCR-VL-1.6-GGUF-mmproj.gguf", directory);
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return false;
    }
    (void)fwrite("GGUF", 1, 4, file);
    pipeline_write_u32(file, 3u);
    pipeline_write_u64(file, 0u);
    pipeline_write_u64(file, 1u);
    pipeline_write_string(file, "clip.vision.image_max_pixels");
    pipeline_write_u32(file, type);
    if (type == 4u) {
        pipeline_write_u32(file, (uint32_t)value);
    } else {
        pipeline_write_u64(file, value);
    }
    return fclose(file) == 0;
}

static void test_pipeline_resolve_max_pixels(void) {
    char error[256];
    uint32_t max_pixels = 0;
    CHECK(pipeline_resolve_max_pixels("third_party", 123u, &max_pixels, error, sizeof error));
    CHECK_U64_EQ(max_pixels, 123u);

    CHECK(platform_create_directories("tests/tmp/tp/PaddleOCR-VL-1.6-GGUF"));
    CHECK(pipeline_build_mmproj("tests/tmp/tp", 4u, 1003520u));
    CHECK(pipeline_resolve_max_pixels("tests/tmp/tp", 0u, &max_pixels, error, sizeof error));
    CHECK_U64_EQ(max_pixels, 1003520u);

    CHECK(pipeline_build_mmproj("tests/tmp/tp", 4u, 0u));
    CHECK(!pipeline_resolve_max_pixels("tests/tmp/tp", 0u, &max_pixels, error, sizeof error));
    CHECK(strstr(error, "--max-pixels") != NULL);

    CHECK(pipeline_build_mmproj("tests/tmp/tp", 10u, 5000000000ull));
    CHECK(!pipeline_resolve_max_pixels("tests/tmp/tp", 0u, &max_pixels, error, sizeof error));

    CHECK(!pipeline_resolve_max_pixels("tests/tmp/empty-tp", 0u, &max_pixels, error, sizeof error));
    CHECK(strstr(error, "not found") != NULL);
}

static void test_pipeline_file_stem_keeps_non_pdf_extensions(void) {
    /* Stripping is driven by the four characters before the end: an extension
     * that merely looks close to .pdf must stay part of the name. */
    char buffer[64];
    CHECK(pipeline_file_stem("notes.pbf", buffer, sizeof buffer));
    CHECK_STR_EQ(buffer, "notes.pbf");
    CHECK(pipeline_file_stem("notes.pdx", buffer, sizeof buffer));
    CHECK_STR_EQ(buffer, "notes.pdx");
    CHECK(pipeline_file_stem("notes.PDX", buffer, sizeof buffer));
    CHECK_STR_EQ(buffer, "notes.PDX");
    CHECK(pipeline_file_stem("notes.pDf", buffer, sizeof buffer));
    CHECK_STR_EQ(buffer, "notes");
}

static bool pipeline_build_mmproj_with_key(const char *directory, const char *key, uint32_t value) {
    char path[512];
    (void)snprintf(path, sizeof path, "%s\\PaddleOCR-VL-1.6-GGUF\\PaddleOCR-VL-1.6-GGUF-mmproj.gguf", directory);
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return false;
    }
    (void)fwrite("GGUF", 1, 4, file);
    pipeline_write_u32(file, 3u);
    pipeline_write_u64(file, 0u);
    pipeline_write_u64(file, 1u);
    pipeline_write_string(file, key);
    pipeline_write_u32(file, 4u);
    pipeline_write_u32(file, value);
    return fclose(file) == 0;
}

static void test_pipeline_resolve_max_pixels_reports_bad_files(void) {
    char error[512];
    uint32_t max_pixels = 0;

    /* A third-party directory that cannot be addressed at all. */
    static char long_directory[1200];
    memset(long_directory, 'd', sizeof long_directory - 1u);
    long_directory[sizeof long_directory - 1u] = '\0';
    CHECK(!pipeline_resolve_max_pixels(long_directory, 0u, &max_pixels, error, sizeof error));
    CHECK(strstr(error, "too long") != NULL);

    /* A projector whose header cannot be read. */
    CHECK(platform_create_directories("tests/tmp/badtp/PaddleOCR-VL-1.6-GGUF"));
    FILE *file = fopen("tests/tmp/badtp\\PaddleOCR-VL-1.6-GGUF\\PaddleOCR-VL-1.6-GGUF-mmproj.gguf", "wb");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(fwrite("NOPE", 1, 4, file) == 4u);
        CHECK(fclose(file) == 0);
    }
    CHECK(!pipeline_resolve_max_pixels("tests/tmp/badtp", 0u, &max_pixels, error, sizeof error));
    CHECK(strstr(error, "could not read the pixel budget") != NULL);

    /* A readable projector that simply has no budget key. */
    CHECK(pipeline_build_mmproj_with_key("tests/tmp/badtp", "clip.vision.image_min_pixels", 112896u));
    CHECK(!pipeline_resolve_max_pixels("tests/tmp/badtp", 0u, &max_pixels, error, sizeof error));
    CHECK(strstr(error, "has no clip.vision.image_max_pixels") != NULL);

    /* The key is there and usable. */
    CHECK(pipeline_build_mmproj_with_key("tests/tmp/badtp", "clip.vision.image_max_pixels", 777u));
    CHECK(pipeline_resolve_max_pixels("tests/tmp/badtp", 0u, &max_pixels, error, sizeof error));
    CHECK_U64_EQ(max_pixels, 777u);
}

void register_pipeline_tests(void) {
    test_register("pipeline_select_range", test_pipeline_select_range);
    test_register("pipeline_work_directory", test_pipeline_work_directory);
    test_register("pipeline_file_stem", test_pipeline_file_stem);
    test_register("pipeline_file_stem_keeps_non_pdf_extensions", test_pipeline_file_stem_keeps_non_pdf_extensions);
    test_register("pipeline_plan_sizes", test_pipeline_plan_sizes);
    test_register("pipeline_plan_sizes_rejects_absurd_budget", test_pipeline_plan_sizes_rejects_absurd_budget);
    test_register("pipeline_request_build", test_pipeline_request_build);
    test_register("pipeline_resolve_max_pixels", test_pipeline_resolve_max_pixels);
    test_register("pipeline_resolve_max_pixels_reports_bad_files", test_pipeline_resolve_max_pixels_reports_bad_files);
}