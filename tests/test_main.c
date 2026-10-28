#include "test.h"

#include "log.h"
#include "platform_win32.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_CAPACITY 4096u

typedef struct {
    const char *name;
    test_function function;
} test_entry;

static test_entry test_entries[TEST_CAPACITY];
static uint32_t test_entry_count = 0;
static uint32_t test_failure_count = 0;

FILE *test_open_temporary(const char *name) {
    static char path[512];
    (void)snprintf(path, sizeof path, "tests/tmp/%s", name);
    return fopen(path, "wb+");
}

void test_register(const char *name, test_function function) {
    if (test_entry_count >= TEST_CAPACITY) {
        printf("test harness overflow: %s\n", name);
        exit(2);
    }
    test_entries[test_entry_count].name = name;
    test_entries[test_entry_count].function = function;
    test_entry_count += 1;
}

void test_check(bool condition, const char *expression, const char *file, uint32_t line, const char *detail) {
    if (condition) {
        return;
    }
    test_failure_count += 1;
    if (detail == NULL) {
        printf("FAIL %s:%u: %s\n", file, line, expression);
    } else {
        printf("FAIL %s:%u: %s (%s)\n", file, line, expression, detail);
    }
}

void test_check_str_eq(const char *actual, const char *expected, const char *file, uint32_t line) {
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return;
    }
    char detail[2048];
    (void)snprintf(detail, sizeof detail, "expected <%s>, actual <%s>",
                   expected == NULL ? "(null)" : expected,
                   actual == NULL ? "(null)" : actual);
    test_check(false, "strings are equal", file, line, detail);
}

void test_check_u64_eq(uint64_t actual, uint64_t expected, const char *file, uint32_t line) {
    if (actual == expected) {
        return;
    }
    char detail[256];
    (void)snprintf(detail, sizeof detail, "expected %llu, actual %llu",
                   (unsigned long long)expected, (unsigned long long)actual);
    test_check(false, "integers are equal", file, line, detail);
}

int main(void) {
    log_init(stderr);
    if (!platform_create_directories("tests/tmp")) {
        printf("could not create tests/tmp\n");
        return 2;
    }

    register_arena_tests();
    register_log_tests();
    register_cli_tests();
    register_base64_tests();
    register_png_tests();
    register_image_tests();
    register_json_tests();
    register_http_message_tests();

    for (uint32_t index = 0; index < test_entry_count; index += 1) {
        uint32_t before = test_failure_count;
        test_entries[index].function();
        printf("%-6s %s\n", test_failure_count == before ? "ok" : "FAILED", test_entries[index].name);
    }

    printf("\n%u tests, %u failures\n", test_entry_count, test_failure_count);
    return test_failure_count == 0 ? 0 : 1;
}