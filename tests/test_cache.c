#include "test.h"

#include "cache.h"
#include "platform_win32.h"

#include <stdio.h>
#include <string.h>

#define TEST_CACHE_WORK_DIR "tests/tmp/cache"
#define TEST_CACHE_KEY 0x0123456789abcdefull

static bool cache_ready(cache *value, uint64_t key) {
    char error[256];
    return cache_init(value, TEST_CACHE_WORK_DIR, key, error, sizeof error);
}

static void test_cache_key_is_stable_and_sensitive(void) {
    uint64_t base = cache_key("book.pdf", 1000u, 300u, 1003520u, 1u);
    CHECK_U64_EQ(base, cache_key("book.pdf", 1000u, 300u, 1003520u, 1u));
    CHECK(cache_key("other.pdf", 1000u, 300u, 1003520u, 1u) != base);
    CHECK(cache_key("book.pdf", 1001u, 300u, 1003520u, 1u) != base);
    CHECK(cache_key("book.pdf", 1000u, 301u, 1003520u, 1u) != base);
    CHECK(cache_key("book.pdf", 1000u, 300u, 1000000u, 1u) != base);
    CHECK(cache_key("book.pdf", 1000u, 300u, 1003520u, 2u) != base);
}

static void test_cache_paths(void) {
    cache value;
    CHECK(cache_ready(&value, TEST_CACHE_KEY));
    CHECK(platform_path_exists(value.directory));
    char path[CACHE_PATH_CAPACITY];
    CHECK(cache_page_path(&value, 0, path, sizeof path));
    CHECK(strstr(path, "00001.txt") != NULL);
    CHECK(cache_page_path(&value, 41, path, sizeof path));
    CHECK(strstr(path, "00042.txt") != NULL);
    CHECK(!cache_page_path(&value, 0, path, 8));
}

static void test_cache_round_trip(void) {
    cache value;
    CHECK(cache_ready(&value, TEST_CACHE_KEY + 1u));
    const char *text = "Page text with \xC2\xA7 and a newline.\n";
    CHECK(cache_write_page(&value, 2, text, (uint64_t)strlen(text)));

    char buffer[256];
    uint64_t length = 0;
    bool found = false;
    CHECK(cache_read_page(&value, 2, buffer, sizeof buffer, &length, &found));
    CHECK(found);
    CHECK_U64_EQ(length, (uint64_t)strlen(text));
    CHECK(memcmp(buffer, text, (size_t)length) == 0);

    CHECK(cache_read_page(&value, 3, buffer, sizeof buffer, &length, &found));
    CHECK(!found);

    CHECK(!cache_read_page(&value, 2, buffer, 8, &length, &found));
}

static void test_cache_empty_page(void) {
    cache value;
    CHECK(cache_ready(&value, TEST_CACHE_KEY + 2u));
    CHECK(cache_write_page(&value, 0, "", 0));
    char buffer[16];
    uint64_t length = 99;
    bool found = false;
    CHECK(cache_read_page(&value, 0, buffer, sizeof buffer, &length, &found));
    CHECK(found);
    CHECK_U64_EQ(length, 0);
}

static void test_cache_manifest(void) {
    cache value;
    CHECK(cache_ready(&value, TEST_CACHE_KEY + 3u));
    const char *manifest = "better-pdf-cache 1\npages 300\n";
    CHECK(cache_write_manifest(&value, manifest, (uint64_t)strlen(manifest)));
    char path[CACHE_PATH_CAPACITY];
    (void)snprintf(path, sizeof path, "%s\\manifest.txt", value.directory);
    CHECK(platform_path_exists(path));
}

static void test_cache_init_rejects_long_work_dir(void) {
    cache value;
    char error[64];
    char long_path[1200];
    memset(long_path, 'a', sizeof long_path);
    long_path[sizeof long_path - 1u] = '\0';
    CHECK(!cache_init(&value, long_path, TEST_CACHE_KEY, error, sizeof error));
    CHECK(!value.ready);
}


static void test_cache_init_reports_directory_failure(void) {
    cache value;
    char error[128];
    CHECK(platform_write_file("tests/tmp/cache-blocker", "x", 1));
    CHECK(!cache_init(&value, "tests/tmp/cache-blocker", TEST_CACHE_KEY, error, sizeof error));
    CHECK(!value.ready);
    CHECK(strstr(error, "could not create") != NULL);
}


static void test_cache_write_replaces_without_leaving_temporary(void) {
    cache value;
    CHECK(cache_ready(&value, TEST_CACHE_KEY));
    CHECK(cache_write_page(&value, 7u, "first", 5u));
    CHECK(cache_write_page(&value, 7u, "second", 6u));

    char path[CACHE_PATH_CAPACITY];
    CHECK(cache_page_path(&value, 7u, path, sizeof path));
    char temporary[CACHE_PATH_CAPACITY];
    (void)snprintf(temporary, sizeof temporary, "%s.tmp", path);
    CHECK(platform_path_exists(path));
    CHECK(!platform_path_exists(temporary));

    char text[32];
    uint64_t length = 0;
    bool found = false;
    CHECK(cache_read_page(&value, 7u, text, sizeof text, &length, &found));
    CHECK(found);
    CHECK_U64_EQ(length, 6u);
    CHECK(memcmp(text, "second", 6u) == 0);
}

void register_cache_tests(void) {
    test_register("cache_key_is_stable_and_sensitive", test_cache_key_is_stable_and_sensitive);
    test_register("cache_paths", test_cache_paths);
    test_register("cache_round_trip", test_cache_round_trip);
    test_register("cache_empty_page", test_cache_empty_page);
    test_register("cache_manifest", test_cache_manifest);
    test_register("cache_init_rejects_long_work_dir", test_cache_init_rejects_long_work_dir);
    test_register("cache_init_reports_directory_failure", test_cache_init_reports_directory_failure);
    test_register("cache_write_replaces_without_leaving_temporary", test_cache_write_replaces_without_leaving_temporary);
}