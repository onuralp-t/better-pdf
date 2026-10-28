#ifndef BETTER_PDF_TYPST_RUN_H
#define BETTER_PDF_TYPST_RUN_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Runs the vendored typst compiler on the generated markup.
 *
 * typst writes its PDF inside the work directory: the compiler refuses to write
 * outside the project root, and keeping the intermediate file next to the
 * markup makes a failed run easy to inspect. A failed compile is retried once
 * without system fonts, which is the usual cause of failure on a machine where
 * fonts are restricted.
 */

typedef struct {
    const char *executable;
    const char *work_dir;
    const char *source_path;
    const char *pdf_path;
    const char *log_path;
} typst_run_config;

bool typst_run(const typst_run_config *config, const char *source, uint64_t source_length, char *error, uint64_t error_capacity);

#endif