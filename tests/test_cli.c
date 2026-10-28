#include "test.h"

#include "cli.h"

#include <string.h>

static cli_status parse(uint32_t count, const char *const *arguments, cli_config *config) {
    static char error[512];
    error[0] = '\0';
    return cli_parse(count, arguments, config, error, sizeof error);
}

static void test_cli_defaults(void) {
    const char *arguments[] = { "better-pdf", "input.pdf", "output.pdf" };
    cli_config config;
    CHECK(parse(3, arguments, &config) == CLI_STATUS_OK);
    CHECK_STR_EQ(config.input_path, "input.pdf");
    CHECK_STR_EQ(config.output_path, "output.pdf");
    CHECK(config.work_dir == NULL);
    CHECK_STR_EQ(config.third_party_dir, "third_party");
    CHECK(config.server_url == NULL);
    CHECK(config.title == NULL);
    CHECK_U64_EQ(config.first_page, 1);
    CHECK_U64_EQ(config.last_page, 0);
    CHECK_U64_EQ(config.port, 18080);
    CHECK_U64_EQ(config.max_pixels, 0);
    CHECK_U64_EQ(config.body_size_pt10, 105);
    CHECK(!config.no_outline);
    CHECK(!config.show_help);
}

static void test_cli_all_options(void) {
    const char *arguments[] = {
        "better-pdf", "--pages", "5-9", "--work-dir", "w", "--third-party-dir", "tp",
        "--server-url", "http://127.0.0.1:8080/v1", "--port", "9000", "--max-pixels", "2000000",
        "--body-pt", "11.5", "--title", "A Book", "--no-outline", "in.pdf", "out.pdf"
    };
    cli_config config;
    CHECK(parse((uint32_t)(sizeof arguments / sizeof arguments[0]), arguments, &config) == CLI_STATUS_OK);
    CHECK_STR_EQ(config.input_path, "in.pdf");
    CHECK_STR_EQ(config.output_path, "out.pdf");
    CHECK_STR_EQ(config.work_dir, "w");
    CHECK_STR_EQ(config.third_party_dir, "tp");
    CHECK_STR_EQ(config.server_url, "http://127.0.0.1:8080/v1");
    CHECK_STR_EQ(config.title, "A Book");
    CHECK_U64_EQ(config.first_page, 5);
    CHECK_U64_EQ(config.last_page, 9);
    CHECK_U64_EQ(config.port, 9000);
    CHECK_U64_EQ(config.max_pixels, 2000000);
    CHECK_U64_EQ(config.body_size_pt10, 115);
    CHECK(config.no_outline);
    CHECK(!config.show_help);
}

static void test_cli_help_needs_no_paths(void) {
    const char *arguments[] = { "better-pdf", "--help" };
    cli_config config;
    CHECK(parse(2, arguments, &config) == CLI_STATUS_OK);
    CHECK(config.show_help);
    CHECK(config.input_path == NULL);
}

static void test_cli_short_help(void) {
    const char *arguments[] = { "better-pdf", "-h" };
    cli_config config;
    CHECK(parse(2, arguments, &config) == CLI_STATUS_OK);
    CHECK(config.show_help);
}

static void test_cli_missing_paths_fails(void) {
    const char *arguments[] = { "better-pdf", "input.pdf" };
    cli_config config;
    CHECK(parse(2, arguments, &config) == CLI_STATUS_ERROR);
}

static void test_cli_extra_positional_fails(void) {
    const char *arguments[] = { "better-pdf", "a.pdf", "b.pdf", "c.pdf" };
    cli_config config;
    CHECK(parse(4, arguments, &config) == CLI_STATUS_ERROR);
}

static void test_cli_missing_value_fails(void) {
    const char *arguments[] = { "better-pdf", "in.pdf", "out.pdf", "--port" };
    cli_config config;
    CHECK(parse(4, arguments, &config) == CLI_STATUS_ERROR);
}

static void test_cli_unknown_option_fails(void) {
    const char *arguments[] = { "better-pdf", "in.pdf", "out.pdf", "--wat" };
    cli_config config;
    CHECK(parse(4, arguments, &config) == CLI_STATUS_ERROR);
}

static void test_cli_single_page(void) {
    const char *arguments[] = { "better-pdf", "--pages", "7", "in.pdf", "out.pdf" };
    cli_config config;
    CHECK(parse(5, arguments, &config) == CLI_STATUS_OK);
    CHECK_U64_EQ(config.first_page, 7);
    CHECK_U64_EQ(config.last_page, 7);
}

static void test_cli_page_range_rejects(void) {
    const char *bad[] = { "0", "0-1", "5-3", "5-", "-5", "1-2-3", "a-2", "1-b", " ", "" };
    for (uint32_t index = 0; index < (uint32_t)(sizeof bad / sizeof bad[0]); index += 1) {
        const char *arguments[] = { "better-pdf", "--pages", bad[index], "in.pdf", "out.pdf" };
        cli_config config;
        CHECK(parse(5, arguments, &config) == CLI_STATUS_ERROR);
    }
}

static void test_cli_page_range_long_left_fails(void) {
    const char *arguments[] = { "better-pdf", "--pages", "1234567890123456789012345678901234567890-2", "in.pdf", "out.pdf" };
    cli_config config;
    CHECK(parse(5, arguments, &config) == CLI_STATUS_ERROR);
}

static void test_cli_page_range_overflow_fails(void) {
    const char *arguments[] = { "better-pdf", "--pages", "1-99999999999", "in.pdf", "out.pdf" };
    cli_config config;
    CHECK(parse(5, arguments, &config) == CLI_STATUS_ERROR);
}

static void test_cli_port_validation(void) {
    const char *bad[] = { "0", "65536", "x", "" };
    for (uint32_t index = 0; index < (uint32_t)(sizeof bad / sizeof bad[0]); index += 1) {
        const char *arguments[] = { "better-pdf", "--port", bad[index], "in.pdf", "out.pdf" };
        cli_config config;
        CHECK(parse(5, arguments, &config) == CLI_STATUS_ERROR);
    }
    const char *arguments[] = { "better-pdf", "--port", "65535", "in.pdf", "out.pdf" };
    cli_config config;
    CHECK(parse(5, arguments, &config) == CLI_STATUS_OK);
    CHECK_U64_EQ(config.port, 65535);
}

static void test_cli_max_pixels_validation(void) {
    const char *bad[] = { "0", "16383", "268435457", "abc" };
    for (uint32_t index = 0; index < (uint32_t)(sizeof bad / sizeof bad[0]); index += 1) {
        const char *arguments[] = { "better-pdf", "--max-pixels", bad[index], "in.pdf", "out.pdf" };
        cli_config config;
        CHECK(parse(5, arguments, &config) == CLI_STATUS_ERROR);
    }
    const char *arguments[] = { "better-pdf", "--max-pixels", "16384", "in.pdf", "out.pdf" };
    cli_config config;
    CHECK(parse(5, arguments, &config) == CLI_STATUS_OK);
    CHECK_U64_EQ(config.max_pixels, 16384);
}

static void test_cli_body_pt_parsing(void) {
    const char *arguments[] = { "better-pdf", "--body-pt", "10", "in.pdf", "out.pdf" };
    cli_config config;
    CHECK(parse(5, arguments, &config) == CLI_STATUS_OK);
    CHECK_U64_EQ(config.body_size_pt10, 100);

    const char *fraction[] = { "better-pdf", "--body-pt", "12.25", "in.pdf", "out.pdf" };
    CHECK(parse(5, fraction, &config) == CLI_STATUS_ERROR);

    /* The whole part is capped at 1000, so a fifth digit has to stop the scan,
     * and the character after the whole part must be the decimal point. */
    const char *bad[] = { "0.4", "24.1", "x", "10.", ".5", "-3", "", "99999", "10x", "10.x" };
    for (uint32_t index = 0; index < (uint32_t)(sizeof bad / sizeof bad[0]); index += 1) {
        const char *arguments_bad[] = { "better-pdf", "--body-pt", bad[index], "in.pdf", "out.pdf" };
        CHECK(parse(5, arguments_bad, &config) == CLI_STATUS_ERROR);
    }
}

static void test_cli_usage_text(void) {
    FILE *sink = test_open_temporary("cli_usage.txt");
    CHECK(sink != NULL);
    cli_print_usage(sink);
    fflush(sink);
    (void)fseek(sink, 0, SEEK_SET);
    char buffer[4096];
    size_t read = fread(buffer, 1, sizeof buffer - 1u, sink);
    buffer[read] = '\0';
    CHECK(strstr(buffer, "usage: better-pdf") != NULL);
    CHECK(strstr(buffer, "--server-url") != NULL);
    fclose(sink);
}

static void test_cli_no_paths_fails(void) {
    const char *arguments[] = { "better-pdf" };
    cli_config config;
    CHECK(parse(1, arguments, &config) == CLI_STATUS_ERROR);
}

static void test_cli_unknown_option_with_value_fails(void) {
    /* An unknown long option is reported even when a value follows it. */
    const char *arguments[] = { "better-pdf", "--wat", "5", "in.pdf", "out.pdf" };
    cli_config config;
    CHECK(parse(5, arguments, &config) == CLI_STATUS_ERROR);
}

static void test_cli_dash_is_positional(void) {
    /* A lone dash is a file name, not an option. */
    const char *arguments[] = { "better-pdf", "-", "out.pdf" };
    cli_config config;
    CHECK(parse(3, arguments, &config) == CLI_STATUS_OK);
    CHECK_STR_EQ(config.input_path, "-");
    CHECK_STR_EQ(config.output_path, "out.pdf");
}

void register_cli_tests(void) {
    test_register("cli_defaults", test_cli_defaults);
    test_register("cli_all_options", test_cli_all_options);
    test_register("cli_help_needs_no_paths", test_cli_help_needs_no_paths);
    test_register("cli_short_help", test_cli_short_help);
    test_register("cli_missing_paths_fails", test_cli_missing_paths_fails);
    test_register("cli_extra_positional_fails", test_cli_extra_positional_fails);
    test_register("cli_missing_value_fails", test_cli_missing_value_fails);
    test_register("cli_unknown_option_fails", test_cli_unknown_option_fails);
    test_register("cli_single_page", test_cli_single_page);
    test_register("cli_page_range_rejects", test_cli_page_range_rejects);
    test_register("cli_page_range_long_left_fails", test_cli_page_range_long_left_fails);
    test_register("cli_page_range_overflow_fails", test_cli_page_range_overflow_fails);
    test_register("cli_port_validation", test_cli_port_validation);
    test_register("cli_max_pixels_validation", test_cli_max_pixels_validation);
    test_register("cli_body_pt_parsing", test_cli_body_pt_parsing);
    test_register("cli_usage_text", test_cli_usage_text);
    test_register("cli_no_paths_fails", test_cli_no_paths_fails);
    test_register("cli_unknown_option_with_value_fails", test_cli_unknown_option_with_value_fails);
    test_register("cli_dash_is_positional", test_cli_dash_is_positional);
}