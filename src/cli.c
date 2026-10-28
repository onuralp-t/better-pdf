#include "cli.h"

#include "asserts.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define CLI_DEFAULT_PORT 18080u
#define CLI_DEFAULT_BODY_SIZE_PT10 105u
#define CLI_MIN_BODY_SIZE_PT10 50u
#define CLI_MAX_BODY_SIZE_PT10 240u
#define CLI_MIN_MAX_PIXELS 16384u
#define CLI_MAX_MAX_PIXELS 268435456u

static void cli_set_error(char *error_message, uint64_t capacity, const char *format, ...) {
    ASSERT(error_message != NULL);
    ASSERT(capacity > 0);
    va_list arguments;
    va_start(arguments, format);
    (void)vsnprintf(error_message, (size_t)capacity, format, arguments);
    va_end(arguments);
}

static bool cli_parse_u32(const char *text, uint32_t *value) {
    ASSERT(text != NULL);
    ASSERT(value != NULL);

    if (text[0] == '\0') {
        return false;
    }
    uint64_t accumulator = 0;
    bool ok = true;
    for (uint64_t index = 0; text[index] != '\0' && ok; index += 1) {
        char character = text[index];
        if (character < '0' || character > '9') {
            ok = false;
        } else {
            accumulator = accumulator * 10u + (uint64_t)(character - '0');
            if (accumulator > (uint64_t)UINT32_MAX) {
                ok = false;
            }
        }
    }
    if (ok) {
        *value = (uint32_t)accumulator;
    }
    return ok;
}

static bool cli_parse_page_range(const char *text, uint32_t *first_page, uint32_t *last_page) {
    ASSERT(text != NULL);
    ASSERT(first_page != NULL);
    ASSERT(last_page != NULL);

    const char *separator = strchr(text, '-');
    if (separator == NULL) {
        uint32_t page = 0;
        if (!cli_parse_u32(text, &page) || page == 0) {
            return false;
        }
        *first_page = page;
        *last_page = page;
        return true;
    }

    uint64_t left_length = (uint64_t)(separator - text);
    if (left_length == 0 || left_length >= 32) {
        return false;
    }
    char left[32];
    memcpy(left, text, (size_t)left_length);
    left[left_length] = '\0';

    uint32_t first = 0;
    uint32_t last = 0;
    if (!cli_parse_u32(left, &first) || !cli_parse_u32(separator + 1, &last)) {
        return false;
    }
    if (first == 0 || last < first) {
        return false;
    }
    *first_page = first;
    *last_page = last;
    return true;
}

static bool cli_parse_pt10(const char *text, uint32_t *value) {
    ASSERT(text != NULL);
    ASSERT(value != NULL);

    uint32_t whole = 0;
    bool ok = true;
    uint64_t index = 0;
    for (; text[index] >= '0' && text[index] <= '9' && ok; index += 1) {
        whole = whole * 10u + (uint32_t)(text[index] - '0');
        if (whole > 1000u) {
            ok = false;
        }
    }
    if (index == 0 || !ok) {
        return false;
    }
    if (text[index] == '\0') {
        *value = whole * 10u;
        return true;
    }
    if (text[index] != '.') {
        return false;
    }
    index += 1;
    if (text[index] < '0' || text[index] > '9' || text[index + 1] != '\0') {
        return false;
    }
    *value = whole * 10u + (uint32_t)(text[index] - '0');
    return true;
}

cli_status cli_parse(uint32_t argument_count, const char *const *arguments, cli_config *config, char *error_message, uint64_t error_capacity) {
    ASSERT(argument_count > 0);
    ASSERT(arguments != NULL);
    ASSERT(config != NULL);
    ASSERT(error_message != NULL);
    ASSERT(error_capacity > 0);

    config->input_path = NULL;
    config->output_path = NULL;
    config->work_dir = NULL;
    config->third_party_dir = NULL;
    config->server_url = NULL;
    config->title = NULL;
    config->first_page = 1;
    config->last_page = 0;
    config->port = (uint16_t)CLI_DEFAULT_PORT;
    config->max_pixels = 0;
    config->body_size_pt10 = CLI_DEFAULT_BODY_SIZE_PT10;
    config->no_outline = false;
    config->show_help = false;

    uint32_t index = 1;
    while (index < argument_count) {
        const char *argument = arguments[index];
        ASSERT(argument != NULL);

        if (argument[0] != '-' || argument[1] == '\0') {
            if (config->input_path == NULL) {
                config->input_path = argument;
            } else if (config->output_path == NULL) {
                config->output_path = argument;
            } else {
                cli_set_error(error_message, error_capacity, "unexpected argument: %s", argument);
                return CLI_STATUS_ERROR;
            }
            index += 1;
            continue;
        }

        if (strcmp(argument, "--help") == 0 || strcmp(argument, "-h") == 0) {
            config->show_help = true;
            index += 1;
            continue;
        }
        if (strcmp(argument, "--no-outline") == 0) {
            config->no_outline = true;
            index += 1;
            continue;
        }

        if (index + 1u >= argument_count) {
            cli_set_error(error_message, error_capacity, "option %s requires a value", argument);
            return CLI_STATUS_ERROR;
        }
        const char *value = arguments[index + 1];
        ASSERT(value != NULL);

        if (strcmp(argument, "--pages") == 0) {
            if (!cli_parse_page_range(value, &config->first_page, &config->last_page)) {
                cli_set_error(error_message, error_capacity, "invalid page range: %s", value);
                return CLI_STATUS_ERROR;
            }
        } else if (strcmp(argument, "--work-dir") == 0) {
            config->work_dir = value;
        } else if (strcmp(argument, "--third-party-dir") == 0) {
            config->third_party_dir = value;
        } else if (strcmp(argument, "--server-url") == 0) {
            config->server_url = value;
        } else if (strcmp(argument, "--title") == 0) {
            config->title = value;
        } else if (strcmp(argument, "--port") == 0) {
            uint32_t port = 0;
            if (!cli_parse_u32(value, &port) || port == 0 || port > 65535u) {
                cli_set_error(error_message, error_capacity, "invalid port: %s", value);
                return CLI_STATUS_ERROR;
            }
            config->port = (uint16_t)port;
        } else if (strcmp(argument, "--max-pixels") == 0) {
            uint32_t pixels = 0;
            if (!cli_parse_u32(value, &pixels) || pixels < CLI_MIN_MAX_PIXELS || pixels > CLI_MAX_MAX_PIXELS) {
                cli_set_error(error_message, error_capacity, "invalid pixel budget: %s", value);
                return CLI_STATUS_ERROR;
            }
            config->max_pixels = pixels;
        } else if (strcmp(argument, "--body-pt") == 0) {
            uint32_t size = 0;
            if (!cli_parse_pt10(value, &size) || size < CLI_MIN_BODY_SIZE_PT10 || size > CLI_MAX_BODY_SIZE_PT10) {
                cli_set_error(error_message, error_capacity, "invalid body size: %s", value);
                return CLI_STATUS_ERROR;
            }
            config->body_size_pt10 = size;
        } else {
            cli_set_error(error_message, error_capacity, "unknown option: %s", argument);
            return CLI_STATUS_ERROR;
        }
        index += 2;
    }

    if (config->show_help) {
        return CLI_STATUS_OK;
    }
    if (config->input_path == NULL || config->output_path == NULL) {
        cli_set_error(error_message, error_capacity, "expected an input PDF and an output PDF");
        return CLI_STATUS_ERROR;
    }
    return CLI_STATUS_OK;
}

void cli_print_usage(FILE *stream) {
    ASSERT(stream != NULL);
    fprintf(stream,
            "usage: better-pdf <input.pdf> <output.pdf> [options]\n"
            "\n"
            "Rewrites a bloated or scanned book PDF as a small, clean, reflowed PDF.\n"
            "Pages are rendered with pdfium, read by PaddleOCR-VL through llama-server,\n"
            "stored as a document AST, and typeset with typst.\n"
            "\n"
            "options:\n"
            "  --pages <first>-<last>   1-based inclusive page range (default: all)\n"
            "  --work-dir <dir>         work and cache directory (default: <output>.work)\n"
            "  --third-party-dir <dir>  location of pdfium, llama-server, models and typst\n"
            "  --server-url <url>       reuse a running llama-server, e.g. http://127.0.0.1:8080/v1\n"
            "  --port <n>               port for the spawned llama-server (default: 18080)\n"
            "  --max-pixels <n>         override the model's image pixel budget\n"
            "  --body-pt <n>            body font size in points (default: 10.5)\n"
            "  --title <text>           document title override\n"
            "  --no-outline             omit the table of contents\n"
            "  --help                   show this message\n");
}