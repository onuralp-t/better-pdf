#include "test.h"

#include "log.h"

#include <stdio.h>
#include <string.h>

static void log_capture(FILE *sink, char *buffer, uint64_t capacity) {
    fflush(sink);
    (void)fseek(sink, 0, SEEK_SET);
    size_t read = fread(buffer, 1, (size_t)(capacity - 1u), sink);
    buffer[read] = '\0';
}

static void test_log_formats_info_and_warn(void) {
    CHECK(log_ready());
    FILE *sink = test_open_temporary("log_info.txt");
    CHECK(sink != NULL);
    log_set_sink(sink);
    LOG("plain message %u", 7u);
    WARN("careful %s", "now");
    static char buffer[4096];
    log_capture(sink, buffer, sizeof buffer);
    CHECK(strstr(buffer, "LOG") != NULL);
    CHECK(strstr(buffer, "plain message 7") != NULL);
    CHECK(strstr(buffer, "WARN") != NULL);
    CHECK(strstr(buffer, "careful now") != NULL);
    log_set_sink(stderr);
    fclose(sink);
}

static void test_log_error_includes_location(void) {
    FILE *sink = test_open_temporary("log_error.txt");
    CHECK(sink != NULL);
    log_set_sink(sink);
    ERROR("broken %d", 42);
    static char buffer[4096];
    log_capture(sink, buffer, sizeof buffer);
    CHECK(strstr(buffer, "ERROR") != NULL);
    CHECK(strstr(buffer, "test_log.c:") != NULL);
    CHECK(strstr(buffer, "broken 42") != NULL);
    log_set_sink(stderr);
    fclose(sink);
}

static void test_log_truncates_long_messages(void) {
    FILE *sink = test_open_temporary("log_long.txt");
    CHECK(sink != NULL);
    log_set_sink(sink);
    static char huge[4096];
    memset(huge, 'x', sizeof huge - 1u);
    huge[sizeof huge - 1u] = '\0';
    LOG("%s", huge);
    static char buffer[8192];
    log_capture(sink, buffer, sizeof buffer);
    CHECK(strstr(buffer, "xxxx") != NULL);
    CHECK(strlen(buffer) < 2048u);
    log_set_sink(stderr);
    fclose(sink);
}

void register_log_tests(void) {
    test_register("log_formats_info_and_warn", test_log_formats_info_and_warn);
    test_register("log_error_includes_location", test_log_error_includes_location);
    test_register("log_truncates_long_messages", test_log_truncates_long_messages);
}