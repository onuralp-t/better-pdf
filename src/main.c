#include "arena.h"
#include "asserts.h"
#include "cli.h"
#include "document.h"
#include "log.h"
#include "pdf_input.h"
#include "pipeline.h"
#include "pipeline_run.h"
#include "platform_win32.h"

#include <stdio.h>
#include <stdlib.h>

#define MAIN_ARGUMENT_CAPACITY 65536u
#define MAIN_MAX_ARGUMENTS 128u

static uint8_t *main_slice(arena *scratch, uint64_t bytes, uint64_t alignment) {
    uint8_t *slice = arena_alloc(scratch, bytes, alignment);
    ASSERT(slice != NULL);
    return slice;
}

int main(void) {
    platform_enable_utf8_console();
    log_init(stderr);
    if (!platform_job_create()) {
        WARN("could not create the child process job; an interrupted run may leave children behind");
    }
    if (!platform_install_interrupt_handler()) {
        WARN("could not install the Ctrl+C handler");
    }
    if (!platform_network_init()) {
        ERROR("could not initialize Winsock");
        return 1;
    }

    static char argument_storage[MAIN_ARGUMENT_CAPACITY];
    const char *arguments[MAIN_MAX_ARGUMENTS];
    uint32_t argument_count = platform_utf8_arguments(arguments, argument_storage, sizeof argument_storage);
    if (argument_count == 0u || argument_count > MAIN_MAX_ARGUMENTS) {
        ERROR("could not read the command line");
        platform_network_shutdown();
        return 1;
    }

    cli_config config;
    char error[512];
    if (cli_parse(argument_count, arguments, &config, error, sizeof error) != CLI_STATUS_OK) {
        ERROR("%s", error);
        cli_print_usage(stderr);
        platform_network_shutdown();
        return 1;
    }
    if (config.show_help) {
        cli_print_usage(stdout);
        platform_network_shutdown();
        return 0;
    }

    pdf_input pdf;
    if (!pdf_input_open(&pdf, config.input_path, error, sizeof error)) {
        ERROR("%s", error);
        platform_network_shutdown();
        return 1;
    }
    uint32_t page_count = pdf_input_page_count(&pdf);
    uint32_t last_page = config.last_page == 0u ? page_count : config.last_page;
    if (config.first_page > page_count || last_page > page_count || config.first_page > last_page) {
        ERROR("page range %u-%u is outside this %u page document", config.first_page, last_page, page_count);
        pdf_input_close(&pdf);
        platform_network_shutdown();
        return 1;
    }

    uint32_t max_pixels = 0;
    if (!pipeline_resolve_max_pixels(config.third_party_dir, config.max_pixels, &max_pixels, error, sizeof error)) {
        ERROR("%s", error);
        pdf_input_close(&pdf);
        platform_network_shutdown();
        return 1;
    }

    pipeline_sizes sizes;
    if (!pipeline_plan_sizes(page_count, max_pixels, &sizes)) {
        ERROR("a %u page document at %u pixels needs more than 2 GiB of memory", page_count, max_pixels);
        pdf_input_close(&pdf);
        platform_network_shutdown();
        return 1;
    }

    uint8_t *memory = malloc((size_t)sizes.total_bytes);
    if (memory == NULL) {
        ERROR("could not allocate %llu bytes", (unsigned long long)sizes.total_bytes);
        pdf_input_close(&pdf);
        platform_network_shutdown();
        return 1;
    }

    arena document_region;
    arena_init(&document_region, memory, sizes.document_bytes);
    uint32_t *page_widths = (uint32_t *)main_slice(&document_region, (uint64_t)page_count * 4u, 4u);
    uint32_t *page_heights = (uint32_t *)main_slice(&document_region, (uint64_t)page_count * 4u, 4u);
    uint64_t document_offset = document_region.used;

    arena scratch;
    arena_init(&scratch, memory + sizes.document_bytes, sizes.total_bytes - sizes.document_bytes);
    document book;
    pipeline_buffers buffers;
    buffers.doc = &book;
    buffers.doc_memory = memory + document_offset;
    buffers.doc_capacity = sizes.document_bytes - document_offset;
    buffers.page_widths = page_widths;
    buffers.page_heights = page_heights;
    buffers.raster = main_slice(&scratch, sizes.raster_bytes, 64u);
    buffers.raster_capacity = sizes.raster_bytes;
    buffers.png = main_slice(&scratch, sizes.png_bytes, 1u);
    buffers.png_capacity = sizes.png_bytes;
    buffers.base64 = (char *)main_slice(&scratch, sizes.base64_bytes, 1u);
    buffers.base64_capacity = sizes.base64_bytes;
    buffers.request = (char *)main_slice(&scratch, sizes.request_bytes, 1u);
    buffers.request_capacity = sizes.request_bytes;
    buffers.response = (char *)main_slice(&scratch, sizes.response_bytes, 1u);
    buffers.response_capacity = sizes.response_bytes;
    buffers.typst = (char *)main_slice(&scratch, sizes.typst_bytes, 1u);
    buffers.typst_capacity = sizes.typst_bytes;
    buffers.page_text = (char *)main_slice(&scratch, sizes.page_text_bytes, 1u);
    buffers.page_text_capacity = sizes.page_text_bytes;

    pipeline_status result = pipeline_run(&config, max_pixels, &buffers, &pdf);

    pdf_input_close(&pdf);
    free(memory);
    platform_network_shutdown();
    return result == PIPELINE_STATUS_OK ? 0 : 2;
}