#include "test.h"

#include "progress.h"

#include <string.h>

static void check_format(const progress *value, const char *expected) {
    char output[160];
    uint64_t written = progress_format(value, output, sizeof output);
    CHECK_U64_EQ(written, strlen(expected));
    CHECK_STR_EQ(output, expected);
}

static void test_progress_empty(void) {
    progress value;
    progress_init(&value, 12u);
    CHECK(progress_average_seconds(&value) == 0.0);
    CHECK(progress_eta_seconds(&value) == 0.0);
    check_format(&value, "[........................] 0/12 pages, 0% | 0.0 s/page | ETA 0s");
}

static void test_progress_average_and_eta(void) {
    progress value;
    progress_init(&value, 10u);
    progress_add_page(&value, 4.0);
    CHECK(progress_average_seconds(&value) == 4.0);
    CHECK(progress_eta_seconds(&value) == 36.0);
    CHECK_U64_EQ(value.completed_pages, 1u);

    progress_add_page(&value, 6.0);
    CHECK(progress_average_seconds(&value) == 5.0);
    CHECK(progress_eta_seconds(&value) == 40.0);
}

static void test_progress_quarter(void) {
    progress value;
    progress_init(&value, 12u);
    for (uint32_t index = 0; index < 3u; index += 1u) {
        progress_add_page(&value, 10.0);
    }
    check_format(&value, "[######..................] 3/12 pages, 25% | 10.0 s/page | ETA 1m 30s");
}

static void test_progress_bar_and_percent_round_down(void) {
    progress value;
    progress_init(&value, 7u);
    progress_add_page(&value, 3.0);
    check_format(&value, "[###.....................] 1/7 pages, 14% | 3.0 s/page | ETA 18s");
}

static void test_progress_complete(void) {
    progress value;
    progress_init(&value, 4u);
    for (uint32_t index = 0; index < 4u; index += 1u) {
        progress_add_page(&value, 2.5);
    }
    CHECK(progress_eta_seconds(&value) == 0.0);
    check_format(&value, "[########################] 4/4 pages, 100% | 2.5 s/page | ETA 0s");
}

static void test_progress_eta_hours(void) {
    progress value;
    progress_init(&value, 1000u);
    progress_add_page(&value, 5.0);
    check_format(&value, "[........................] 1/1000 pages, 0% | 5.0 s/page | ETA 1h 23m");
}

static void test_progress_eta_exact_minute(void) {
    progress value;
    progress_init(&value, 4u);
    progress_add_page(&value, 20.0);
    check_format(&value, "[######..................] 1/4 pages, 25% | 20.0 s/page | ETA 1m 00s");
}

static void test_progress_eta_rounds_up(void) {
    progress value;
    progress_init(&value, 3u);
    progress_add_page(&value, 1.0);
    progress_add_page(&value, 2.0);
    check_format(&value, "[################........] 2/3 pages, 66% | 1.5 s/page | ETA 2s");
}

static void test_progress_capacity(void) {
    progress value;
    progress_init(&value, 12u);
    progress_add_page(&value, 4.0);

    char output[160];
    uint64_t needed = progress_format(&value, output, sizeof output);
    CHECK(needed > 0u);
    CHECK_U64_EQ(progress_format(&value, output, needed), 0u);
    CHECK_U64_EQ(progress_format(&value, output, needed + 1u), needed);
}

void register_progress_tests(void) {
    test_register("progress_empty", test_progress_empty);
    test_register("progress_average_and_eta", test_progress_average_and_eta);
    test_register("progress_quarter", test_progress_quarter);
    test_register("progress_bar_and_percent_round_down", test_progress_bar_and_percent_round_down);
    test_register("progress_complete", test_progress_complete);
    test_register("progress_eta_hours", test_progress_eta_hours);
    test_register("progress_eta_exact_minute", test_progress_eta_exact_minute);
    test_register("progress_eta_rounds_up", test_progress_eta_rounds_up);
    test_register("progress_capacity", test_progress_capacity);
}