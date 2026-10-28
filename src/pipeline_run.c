#include "pipeline_run.h"

#include "asserts.h"
#include "base64.h"
#include "cache.h"
#include "document.h"
#include "geometry.h"
#include "image.h"
#include "log.h"
#include "ocr_client.h"
#include "platform_win32.h"
#include "png.h"
#include "server_process.h"
#include "typst_emit.h"
#include "typst_run.h"

#include <stdio.h>
#include <string.h>

#define PIPELINE_PATH_CAPACITY 1024u
#define PIPELINE_SERVER_TIMEOUT_SECONDS 300u
#define PIPELINE_PROGRESS_EVERY 10u

static bool pipeline_join(char *output, uint64_t capacity, const char *directory, const char *relative) {
    ASSERT(output != NULL);
    ASSERT(directory != NULL);
    ASSERT(relative != NULL);
    int written = snprintf(output, (size_t)capacity, "%s\\%s", directory, relative);
    return written > 0 && (uint64_t)written < capacity;
}

static bool pipeline_output_directory(const char *output_path, char *buffer, uint64_t capacity) {
    ASSERT(output_path != NULL);
    ASSERT(buffer != NULL);

    const char *last_separator = NULL;
    for (const char *cursor = output_path; *cursor != '\0'; cursor += 1) {
        if (*cursor == '\\' || *cursor == '/') {
            last_separator = cursor;
        }
    }
    uint64_t length = last_separator == NULL ? 0u : (uint64_t)(last_separator - output_path);
    if (length == 0u) {
        const char *current = ".";
        if (2u > capacity) {
            return false;
        }
        memcpy(buffer, current, 2u);
        return true;
    }
    if (length + 1u > capacity) {
        return false;
    }
    memcpy(buffer, output_path, (size_t)length);
    buffer[length] = '\0';
    return true;
}

static bool pipeline_recognize_page(uint32_t max_pixels, const pipeline_buffers *buffers, pdf_input *pdf, const ocr_endpoint *endpoint, uint32_t page_index, uint64_t *text_length) {
    ASSERT(buffers != NULL);
    ASSERT(text_length != NULL);

    char error[512];
    image_size size;
    if (!image_plan(buffers->page_widths[page_index], buffers->page_heights[page_index], max_pixels, &size)) {
        ERROR("page %u has a size that cannot be rendered", page_index + 1u);
        return false;
    }
    if (!pdf_input_render_page(pdf, page_index, size.width, size.height, buffers->raster, (uint64_t)size.width * 4u, error, sizeof error)) {
        ERROR("%s", error);
        return false;
    }

    image_analysis analysis;
    image_analyze(buffers->raster, size.width, size.height, (uint64_t)size.width * 4u, &analysis);
    if (analysis.kind == PAGE_KIND_BLANK) {
        WARN("page %u looks blank: %llu of %llu pixels inked", page_index + 1u, (unsigned long long)analysis.ink_pixels, (unsigned long long)analysis.pixel_count);
        *text_length = 0;
        return true;
    }
    if (analysis.kind == PAGE_KIND_DARK) {
        WARN("page %u is almost solid ink: %llu of %llu pixels inked", page_index + 1u, (unsigned long long)analysis.ink_pixels, (unsigned long long)analysis.pixel_count);
        *text_length = 0;
        return true;
    }

    image_convert_in_place(buffers->raster, size.width, size.height, analysis.mode);
    png_mode mode = analysis.mode == IMAGE_MODE_GRAY ? PNG_MODE_GRAY : PNG_MODE_RGB;
    uint64_t png_length = png_encode(buffers->raster, size.width, size.height, mode, buffers->png, buffers->png_capacity);
    if (png_length == 0u) {
        ERROR("page %u could not be encoded as PNG", page_index + 1u);
        return false;
    }
    uint64_t base64_length = base64_encode(buffers->png, png_length, buffers->base64, buffers->base64_capacity);
    if (base64_length == 0u) {
        ERROR("page %u could not be encoded as base64", page_index + 1u);
        return false;
    }
    uint64_t request_length = pipeline_request_build(buffers->base64, base64_length, buffers->request, buffers->request_capacity);
    if (request_length == 0u) {
        ERROR("page %u produced a request that does not fit", page_index + 1u);
        return false;
    }

    uint64_t response_length = 0;
    uint64_t body_offset = 0;
    uint64_t body_length = 0;
    if (!ocr_client_post(endpoint, buffers->request, request_length, buffers->response, buffers->response_capacity, &response_length, &body_offset, &body_length, error, sizeof error)) {
        ERROR("%s", error);
        return false;
    }
    if (!ocr_response_text(buffers->response + body_offset, body_length, buffers->page_text, buffers->page_text_capacity, text_length, error, sizeof error)) {
        ERROR("%s", error);
        return false;
    }
    return true;
}

static const char *pipeline_page_word(uint32_t count) {
    return count == 1u ? "page" : "pages";
}

pipeline_status pipeline_run(const cli_config *config, uint32_t max_pixels, const pipeline_buffers *buffers, pdf_input *pdf) {
    ASSERT(config != NULL);
    ASSERT(buffers != NULL);
    ASSERT(pdf != NULL);

    char error[512];
    pipeline_status status = PIPELINE_STATUS_ERROR;
    server_process server;
    server.process = NULL;
    server.thread = NULL;
    bool server_started = false;
    double started = platform_monotonic_seconds();

    uint32_t page_count = pdf_input_page_count(pdf);
    uint32_t first_index = 0;
    uint32_t last_index = 0;
    if (!pipeline_select_range(page_count, config->first_page, config->last_page, &first_index, &last_index)) {
        ERROR("page range %u-%u is outside this %u page document", config->first_page, config->last_page == 0u ? page_count : config->last_page, page_count);
        return PIPELINE_STATUS_ERROR;
    }

    for (uint32_t index = 0; index < page_count; index += 1) {
        if (!pdf_input_page_size(pdf, index, &buffers->page_widths[index], &buffers->page_heights[index], error, sizeof error)) {
            ERROR("%s", error);
            return PIPELINE_STATUS_ERROR;
        }
    }

    char work_dir[PIPELINE_PATH_CAPACITY];
    if (!pipeline_work_directory(config->output_path, config->work_dir, work_dir, sizeof work_dir)) {
        ERROR("the work directory path is too long");
        return PIPELINE_STATUS_ERROR;
    }
    if (!platform_create_directories(work_dir)) {
        ERROR("could not create the work directory %s", work_dir);
        return PIPELINE_STATUS_ERROR;
    }
    char resolved_work_dir[PIPELINE_PATH_CAPACITY];
    if (!platform_absolute_path(work_dir, resolved_work_dir, sizeof resolved_work_dir)) {
        ERROR("could not resolve the work directory %s", work_dir);
        return PIPELINE_STATUS_ERROR;
    }

    uint64_t input_size = 0;
    if (!platform_file_size(config->input_path, &input_size)) {
        ERROR("could not read the size of %s", config->input_path);
        return PIPELINE_STATUS_ERROR;
    }
    uint64_t key = cache_key(config->input_path, input_size, page_count, max_pixels, CACHE_PIPELINE_VERSION);
    cache page_cache;
    if (!cache_init(&page_cache, resolved_work_dir, key, error, sizeof error)) {
        ERROR("%s", error);
        return PIPELINE_STATUS_ERROR;
    }
    char manifest[1024];
    int manifest_length = snprintf(manifest, sizeof manifest,
        "better-pdf-cache %u\ninput %s\nsize %llu\npages %u\nmax_pixels %u\nkey %016llx\n",
        CACHE_PIPELINE_VERSION, config->input_path, (unsigned long long)input_size, page_count, max_pixels, (unsigned long long)key);
    if (manifest_length < 0 || (uint64_t)manifest_length >= sizeof manifest ||
        !cache_write_manifest(&page_cache, manifest, (uint64_t)manifest_length)) {
        WARN("could not write the cache manifest");
    }

    char server_url[256];
    if (config->server_url != NULL) {
        if ((uint64_t)snprintf(server_url, sizeof server_url, "%s", config->server_url) >= sizeof server_url) {
            ERROR("the server URL is too long");
            return PIPELINE_STATUS_ERROR;
        }
    } else if (snprintf(server_url, sizeof server_url, "http://127.0.0.1:%u/v1", (unsigned)config->port) >= (int)sizeof server_url) {
        ERROR("the server URL is too long");
        return PIPELINE_STATUS_ERROR;
    }
    ocr_endpoint endpoint;
    if (!ocr_endpoint_parse(server_url, &endpoint, error, sizeof error)) {
        ERROR("%s", error);
        return PIPELINE_STATUS_ERROR;
    }

    uint32_t missing = 0;
    for (uint32_t index = first_index; index <= last_index; index += 1) {
        char page_path[CACHE_PATH_CAPACITY];
        if (!cache_page_path(&page_cache, index, page_path, sizeof page_path)) {
            ERROR("the cache path for page %u is too long", index + 1u);
            return PIPELINE_STATUS_ERROR;
        }
        if (!platform_path_exists(page_path)) {
            missing += 1;
        }
    }

    if (missing > 0u && config->server_url == NULL) {
        char executable[PIPELINE_PATH_CAPACITY];
        char model_path[PIPELINE_PATH_CAPACITY];
        char mmproj_path[PIPELINE_PATH_CAPACITY];
        if (!pipeline_join(executable, sizeof executable, config->third_party_dir, "llama-b11968-bin-win-cpu-x64\\llama-server.exe") ||
            !pipeline_join(model_path, sizeof model_path, config->third_party_dir, "PaddleOCR-VL-1.6-GGUF\\PaddleOCR-VL-1.6-GGUF.gguf") ||
            !pipeline_join(mmproj_path, sizeof mmproj_path, config->third_party_dir, "PaddleOCR-VL-1.6-GGUF\\PaddleOCR-VL-1.6-GGUF-mmproj.gguf")) {
            ERROR("the third-party directory path is too long");
            return PIPELINE_STATUS_ERROR;
        }
        char absolute_executable[PIPELINE_PATH_CAPACITY];
        char absolute_model[PIPELINE_PATH_CAPACITY];
        char absolute_mmproj[PIPELINE_PATH_CAPACITY];
        if (!platform_absolute_path(executable, absolute_executable, sizeof absolute_executable) ||
            !platform_absolute_path(model_path, absolute_model, sizeof absolute_model) ||
            !platform_absolute_path(mmproj_path, absolute_mmproj, sizeof absolute_mmproj)) {
            ERROR("could not resolve the third-party paths");
            return PIPELINE_STATUS_ERROR;
        }
        if (server_process_port_ready(config->port)) {
            ERROR("something already answers on port %u; close it, choose another --port, or pass --server-url to reuse it", config->port);
            return PIPELINE_STATUS_ERROR;
        }
        LOG("starting the OCR server for %u uncached %s", missing, pipeline_page_word(missing));
        if (!server_process_start(&server, absolute_executable, absolute_model, absolute_mmproj, config->port, resolved_work_dir, error, sizeof error)) {
            ERROR("%s", error);
            return PIPELINE_STATUS_ERROR;
        }
        server_started = true;
        if (!server_process_wait_ready(&server, config->port, PIPELINE_SERVER_TIMEOUT_SECONDS) || !server_process_running(&server)) {
            ERROR("the OCR server did not start; see %s\\llama-server.log", resolved_work_dir);
            goto done;
        }
        LOG("the OCR server is ready; its output is in %s\\llama-server.log", resolved_work_dir);
    } else if (missing > 0u) {
        LOG("using the OCR server at %s for %u uncached %s", server_url, missing, pipeline_page_word(missing));
    } else {
        LOG("all %u pages are cached; no OCR server is needed", last_index - first_index + 1u);
    }

    if (!document_init(buffers->doc, buffers->doc_memory, buffers->doc_capacity, page_count, document_text_capacity(page_count))) {
        ERROR("the document storage is too small for %u pages", page_count);
        goto done;
    }

    uint32_t recognized = 0;
    uint32_t cached = 0;
    for (uint32_t index = first_index; index <= last_index; index += 1) {
        uint64_t text_length = 0;
        bool found = false;
        if (!cache_read_page(&page_cache, index, buffers->page_text, buffers->page_text_capacity, &text_length, &found)) {
            ERROR("could not read the cached text for page %u", index + 1u);
            goto done;
        }
        if (found) {
            cached += 1;
        } else {
            if (!pipeline_recognize_page(max_pixels, buffers, pdf, &endpoint, index, &text_length)) {
                goto done;
            }
            if (!cache_write_page(&page_cache, index, buffers->page_text, text_length)) {
                ERROR("could not cache the text of page %u", index + 1u);
                goto done;
            }
            recognized += 1;
            LOG("page %u/%u recognized: %llu bytes", index + 1u, page_count, (unsigned long long)text_length);
        }
        if (!document_add_page(buffers->doc, index, buffers->page_text, text_length)) {
            ERROR("the text of page %u does not fit into memory", index + 1u);
            goto done;
        }
    }

    if (server_started) {
        server_process_stop(&server);
        server_started = false;
    }

    char stem[256];
    if (!pipeline_file_stem(config->input_path, stem, sizeof stem)) {
        ERROR("the input file name is too long");
        goto done;
    }
    if (!document_finalize(buffers->doc, stem, config->title)) {
        ERROR("the document could not be assembled");
        goto done;
    }

    page_geometry geometry;
    if (!geometry_from_page_sizes(buffers->page_widths, buffers->page_heights, page_count, config->body_size_pt10, &geometry)) {
        ERROR("could not derive a page size from the input document");
        goto done;
    }

    typst_config layout;
    layout.geometry = geometry;
    layout.title = document_title(buffers->doc);
    layout.title_length = buffers->doc->title_length;
    layout.include_outline = buffers->doc->has_outline && !config->no_outline;

    uint64_t typst_length = typst_emit(buffers->doc, &layout, buffers->typst, buffers->typst_capacity);
    if (typst_length == 0u) {
        ERROR("the generated typst markup does not fit into %llu bytes", (unsigned long long)buffers->typst_capacity);
        goto done;
    }
    LOG("layout: %u pages, %u chapters, %llu bytes of markup", buffers->doc->page_count, buffers->doc->chapter_count, (unsigned long long)typst_length);

    char typst_executable[PIPELINE_PATH_CAPACITY];
    char source_path[PIPELINE_PATH_CAPACITY];
    char pdf_path[PIPELINE_PATH_CAPACITY];
    char log_path[PIPELINE_PATH_CAPACITY];
    char output_directory[PIPELINE_PATH_CAPACITY];
    if (!pipeline_join(typst_executable, sizeof typst_executable, config->third_party_dir, "typst\\typst.exe") ||
        !pipeline_join(source_path, sizeof source_path, resolved_work_dir, "book.typ") ||
        !pipeline_join(pdf_path, sizeof pdf_path, resolved_work_dir, "book.pdf") ||
        !pipeline_join(log_path, sizeof log_path, resolved_work_dir, "typst.log")) {
        ERROR("the work directory path is too long");
        goto done;
    }
    char resolved_typst[PIPELINE_PATH_CAPACITY];
    if (!platform_absolute_path(typst_executable, resolved_typst, sizeof resolved_typst)) {
        ERROR("could not resolve %s", typst_executable);
        goto done;
    }
    typst_run_config compile;
    compile.executable = resolved_typst;
    compile.work_dir = resolved_work_dir;
    compile.source_path = source_path;
    compile.pdf_path = pdf_path;
    compile.log_path = log_path;
    if (!typst_run(&compile, buffers->typst, typst_length, error, sizeof error)) {
        ERROR("%s", error);
        goto done;
    }

    if (!pipeline_output_directory(config->output_path, output_directory, sizeof output_directory)) {
        ERROR("the output path is too long");
        goto done;
    }
    if (!platform_create_directories(output_directory)) {
        ERROR("could not create the output directory %s", output_directory);
        goto done;
    }
    if (!platform_copy_file(pdf_path, config->output_path)) {
        ERROR("could not write %s", config->output_path);
        goto done;
    }

    double elapsed = platform_monotonic_seconds() - started;
    LOG("wrote %s: %u pages, %u recognized, %u cached, %.1f s", config->output_path, last_index - first_index + 1u, recognized, cached, elapsed);
    status = PIPELINE_STATUS_OK;

done:
    if (server_started) {
        server_process_stop(&server);
    }
    return status;
}