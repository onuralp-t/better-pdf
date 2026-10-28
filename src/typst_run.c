#include "typst_run.h"

#include "asserts.h"
#include "platform_win32.h"

#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static bool typst_run_process(const char *command, const char *work_dir, const char *log_path, char *error, uint64_t error_capacity) {
    ASSERT(command != NULL);
    ASSERT(log_path != NULL);

    SECURITY_ATTRIBUTES inherit;
    inherit.nLength = sizeof inherit;
    inherit.lpSecurityDescriptor = NULL;
    inherit.bInheritHandle = TRUE;
    HANDLE log = CreateFileA(log_path, GENERIC_WRITE, FILE_SHARE_READ, &inherit, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (log == INVALID_HANDLE_VALUE) {
        (void)snprintf(error, (size_t)error_capacity, "could not create %s", log_path);
        return false;
    }
    HANDLE input = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    STARTUPINFOA startup;
    PROCESS_INFORMATION info;
    memset(&startup, 0, sizeof startup);
    memset(&info, 0, sizeof info);
    startup.cb = sizeof startup;
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input;
    startup.hStdOutput = log;
    startup.hStdError = log;

    BOOL created = CreateProcessA(NULL, (char *)command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, work_dir, &startup, &info);
    CloseHandle(log);
    if (input != INVALID_HANDLE_VALUE) {
        CloseHandle(input);
    }
    if (created == 0) {
        (void)snprintf(error, (size_t)error_capacity, "could not start typst (error %lu)", GetLastError());
        return false;
    }
    platform_job_assign(info.hProcess);
    (void)WaitForSingleObject(info.hProcess, 120000u);
    DWORD exit_code = 1;
    (void)GetExitCodeProcess(info.hProcess, &exit_code);
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    if (exit_code != 0) {
        (void)snprintf(error, (size_t)error_capacity, "typst failed; see %s", log_path);
        return false;
    }
    return true;
}

static bool typst_try_compile(const typst_run_config *config, const char *extra, char *error, uint64_t error_capacity) {
    char command[2048];
    int written = snprintf(command, sizeof command, "\"%s\" compile%s --root \"%s\" \"%s\" \"%s\"",
        config->executable, extra, config->work_dir, config->source_path, config->pdf_path);
    if (written < 0 || (uint64_t)written >= sizeof command) {
        (void)snprintf(error, (size_t)error_capacity, "typst command line is too long");
        return false;
    }
    return typst_run_process(command, config->work_dir, config->log_path, error, error_capacity);
}

bool typst_run(const typst_run_config *config, const char *source, uint64_t source_length, char *error, uint64_t error_capacity) {
    ASSERT(config != NULL);
    ASSERT(source != NULL);
    ASSERT(error != NULL);

    if (!platform_path_exists(config->executable)) {
        (void)snprintf(error, (size_t)error_capacity, "typst not found at %s", config->executable);
        return false;
    }
    if (!platform_write_file(config->source_path, source, source_length)) {
        (void)snprintf(error, (size_t)error_capacity, "could not write %s", config->source_path);
        return false;
    }
    if (platform_path_exists(config->pdf_path)) {
        (void)platform_remove_file(config->pdf_path);
    }
    if (!typst_try_compile(config, "", error, error_capacity)) {
        if (!typst_try_compile(config, " --ignore-system-fonts", error, error_capacity)) {
            return false;
        }
    }
    if (!platform_path_exists(config->pdf_path)) {
        (void)snprintf(error, (size_t)error_capacity, "typst reported success but produced no PDF");
        return false;
    }
    return true;
}