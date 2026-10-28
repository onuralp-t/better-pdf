#ifndef BETTER_PDF_PIPELINE_RUN_H
#define BETTER_PDF_PIPELINE_RUN_H

#include "cli.h"
#include "pdf_input.h"
#include "pipeline.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PIPELINE_STATUS_OK = 0,
    PIPELINE_STATUS_ERROR = 1
} pipeline_status;

/*
 * The orchestrator: one function that owns the order of operations and does
 * nothing else. Every step either succeeds or reports a specific error, and the
 * single exit path stops the OCR server no matter where the run failed.
 */
pipeline_status pipeline_run(const cli_config *config, uint32_t max_pixels, const pipeline_buffers *buffers, pdf_input *pdf);

#endif