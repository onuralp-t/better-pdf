#ifndef BETTER_PDF_CLI_H
#define BETTER_PDF_CLI_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    const char *input_path;
    const char *output_path;
    const char *work_dir;
    const char *third_party_dir;
    const char *server_url;
    const char *title;
    uint32_t first_page;    /* 1-based, inclusive */
    uint32_t last_page;     /* 1-based, inclusive; 0 means "to the end" */
    uint16_t port;
    uint32_t max_pixels;    /* 0 means "read from the model metadata" */
    uint32_t body_size_pt10;/* body font size in tenths of a point */
    bool no_outline;
    bool show_help;
} cli_config;

typedef enum {
    CLI_STATUS_OK = 0,
    CLI_STATUS_ERROR = 1
} cli_status;

cli_status cli_parse(uint32_t argument_count, const char *const *arguments, cli_config *config, char *error_message, uint64_t error_capacity);
void cli_print_usage(FILE *stream);

#endif