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

static void test_progress_active_format(void) {
    progress value;
    progress_init(&value, 12u);
    char output[192];
    uint64_t written = progress_format_active(&value, 1u, 5.0, output, sizeof output);
    CHECK_STR_EQ(output, "[........................] 0/12 pages, 0% | page 1: 5s | 0.0 s/page | ETA 0s");
    CHECK_U64_EQ(written, strlen(output));

    for (uint32_t index = 0; index < 3u; index += 1u) {
        progress_add_page(&value, 10.0);
    }
    written = progress_format_active(&value, 4u, 17.9, output, sizeof output);
    CHECK_STR_EQ(output, "[######..................] 3/12 pages, 25% | page 4: 17s | 10.0 s/page | ETA 1m 30s");
    CHECK_U64_EQ(written, strlen(output));

    uint64_t needed = progress_format_active(&value, 4u, 17.9, output, sizeof output);
    CHECK_U64_EQ(progress_format_active(&value, 4u, 17.9, output, needed), 0u);
    CHECK_U64_EQ(progress_format_active(&value, 4u, 17.9, output, needed + 1u), needed);
}

static void display_capture(FILE *stream, char *buffer, uint64_t capacity) {
    CHECK(fflush(stream) == 0);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    size_t read = fread(buffer, 1, (size_t)(capacity - 1u), stream);
    buffer[read] = '\0';
}

static void test_progress_display_writes_in_place(void) {
    FILE *stream = test_open_temporary("progress_display.txt");
    CHECK(stream != NULL);
    progress_display display;
    progress_display_init(&display, stream);
    CHECK(progress_display_enabled(&display));

    progress_display_show(&display, "123456");
    progress_display_show(&display, "12");
    progress_display_suspend(&display);
    progress_display_resume(&display);
    progress_display_complete(&display, "done");
    progress_display_suspend(&display);

    char buffer[256];
    display_capture(stream, buffer, sizeof buffer);
    CHECK_STR_EQ(buffer, "\r123456\r12    \r      \r12    \rdone  \n");
    fclose(stream);
}

static void test_progress_display_idle_states(void) {
    FILE *stream = test_open_temporary("progress_display_idle.txt");
    CHECK(stream != NULL);
    progress_display display;
    progress_display_init(&display, stream);

    progress_display_suspend(&display);
    progress_display_resume(&display);
    progress_display_show(&display, "line");
    progress_display_resume(&display);
    progress_display_suspend(&display);
    progress_display_suspend(&display);

    char buffer[128];
    display_capture(stream, buffer, sizeof buffer);
    CHECK_STR_EQ(buffer, "\rline\r    \r");
    fclose(stream);
}

static void test_progress_display_disabled(void) {
    progress_display display;
    progress_display_init(&display, NULL);
    CHECK(!progress_display_enabled(&display));
    progress_display_show(&display, "ignored");
    progress_display_suspend(&display);
    progress_display_resume(&display);
    progress_display_complete(&display, "ignored");
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
    test_register("progress_active_format", test_progress_active_format);
    test_register("progress_display_writes_in_place", test_progress_display_writes_in_place);
    test_register("progress_display_idle_states", test_progress_display_idle_states);
    test_register("progress_display_disabled", test_progress_display_disabled);
}