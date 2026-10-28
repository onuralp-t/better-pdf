#ifndef BETTER_PDF_TEST_H
#define BETTER_PDF_TEST_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef void (*test_function)(void);

FILE *test_open_temporary(const char *name);

void test_register(const char *name, test_function function);
void test_check(bool condition, const char *expression, const char *file, uint32_t line, const char *detail);
void test_check_str_eq(const char *actual, const char *expected, const char *file, uint32_t line);
void test_check_u64_eq(uint64_t actual, uint64_t expected, const char *file, uint32_t line);

#define CHECK(condition) test_check((condition), #condition, __FILE__, __LINE__, NULL)
#define CHECK_DETAIL(condition, detail) test_check((condition), #condition, __FILE__, __LINE__, (detail))
#define CHECK_STR_EQ(actual, expected) test_check_str_eq((actual), (expected), __FILE__, __LINE__)
#define CHECK_U64_EQ(actual, expected) test_check_u64_eq((actual), (expected), __FILE__, __LINE__)

void register_arena_tests(void);
void register_log_tests(void);
void register_cli_tests(void);
void register_base64_tests(void);
void register_png_tests(void);
void register_image_tests(void);
void register_json_tests(void);
void register_http_message_tests(void);
void register_gguf_tests(void);
void register_ocr_text_tests(void);
void register_document_tests(void);
void register_geometry_tests(void);
void register_typst_emit_tests(void);
void register_cache_tests(void);
void register_pipeline_tests(void);
void register_progress_tests(void);

#endif