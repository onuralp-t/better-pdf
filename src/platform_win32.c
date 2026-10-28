#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <wchar.h>

#include "asserts.h"
#include "platform_win32.h"

#include <stdio.h>
#include <string.h>

static bool platform_path_exists_wide(const wchar_t *path) {
    ASSERT(path != NULL);
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

bool platform_utf8_to_utf16(const char *utf8, wchar_t *output, uint32_t capacity) {
    ASSERT(utf8 != NULL);
    ASSERT(output != NULL);
    int converted = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, output, (int)capacity);
    return converted > 0;
}

double platform_monotonic_seconds(void) {
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    if (QueryPerformanceFrequency(&frequency) == 0 || QueryPerformanceCounter(&counter) == 0) {
        return 0.0;
    }
    return (double)counter.QuadPart / (double)frequency.QuadPart;
}

void platform_enable_utf8_console(void) {
    (void)SetConsoleOutputCP(CP_UTF8);
    (void)SetConsoleCP(CP_UTF8);
}

bool platform_network_init(void) {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}

void platform_network_shutdown(void) {
    (void)WSACleanup();
}

static HANDLE platform_job = NULL;

bool platform_job_create(void) {
    if (platform_job != NULL) {
        return true;
    }
    HANDLE job = CreateJobObjectW(NULL, NULL);
    if (job == NULL) {
        return false;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
    memset(&limits, 0, sizeof limits);
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, (DWORD)sizeof limits) == 0) {
        CloseHandle(job);
        return false;
    }
    platform_job = job;
    return true;
}

void platform_job_assign(void *process) {
    if (platform_job == NULL || process == NULL) {
        return;
    }
    (void)AssignProcessToJobObject(platform_job, (HANDLE)process);
}

void platform_job_terminate(void) {
    if (platform_job != NULL) {
        (void)TerminateJobObject(platform_job, 130u);
    }
}

static BOOL WINAPI platform_interrupt_handler(DWORD signal) {
    if (signal != CTRL_C_EVENT && signal != CTRL_BREAK_EVENT && signal != CTRL_CLOSE_EVENT) {
        return FALSE;
    }
    static const char message[] = "\nbetter-pdf: interrupted; stopping child processes\n";
    DWORD written = 0;
    (void)WriteFile(GetStdHandle(STD_ERROR_HANDLE), message, (DWORD)(sizeof message - 1u), &written, NULL);
    platform_job_terminate();
    ExitProcess(130u);
    return TRUE;
}

bool platform_install_interrupt_handler(void) {
    return SetConsoleCtrlHandler(platform_interrupt_handler, TRUE) != 0;
}

bool platform_replace_file(const char *from_path, const char *to_path) {
    ASSERT(from_path != NULL);
    ASSERT(to_path != NULL);
    wchar_t wide_from[PLATFORM_PATH_CAPACITY];
    wchar_t wide_to[PLATFORM_PATH_CAPACITY];
    if (!platform_utf8_to_utf16(from_path, wide_from, PLATFORM_PATH_CAPACITY) || !platform_utf8_to_utf16(to_path, wide_to, PLATFORM_PATH_CAPACITY)) {
        return false;
    }
    return MoveFileExW(wide_from, wide_to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

bool platform_read_file(const char *path, char *buffer, uint64_t capacity, uint64_t *length, bool *found) {
    ASSERT(path != NULL);
    ASSERT(buffer != NULL);
    ASSERT(length != NULL);
    ASSERT(found != NULL);

    wchar_t wide_path[PLATFORM_PATH_CAPACITY];
    if (!platform_utf8_to_utf16(path, wide_path, PLATFORM_PATH_CAPACITY)) {
        return false;
    }

    FILE *file = _wfopen(wide_path, L"rb");
    if (file == NULL) {
        *found = false;
        *length = 0;
        return true;
    }
    *found = true;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    int64_t size = (int64_t)ftell(file);
    if (size < 0 || (uint64_t)size > capacity || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    uint64_t read = (uint64_t)fread(buffer, 1, (size_t)size, file);
    fclose(file);
    if (read != (uint64_t)size) {
        return false;
    }
    *length = (uint64_t)size;
    return true;
}

bool platform_write_file(const char *path, const char *data, uint64_t length) {
    ASSERT(path != NULL);
    ASSERT(data != NULL);

    wchar_t wide_path[PLATFORM_PATH_CAPACITY];
    if (!platform_utf8_to_utf16(path, wide_path, PLATFORM_PATH_CAPACITY)) {
        return false;
    }

    FILE *file = _wfopen(wide_path, L"wb");
    if (file == NULL) {
        return false;
    }
    uint64_t written = (uint64_t)fwrite(data, 1, (size_t)length, file);
    int closed = fclose(file);
    return written == (uint64_t)length && closed == 0;
}

bool platform_path_exists(const char *path) {
    ASSERT(path != NULL);
    wchar_t wide_path[PLATFORM_PATH_CAPACITY];
    if (!platform_utf8_to_utf16(path, wide_path, PLATFORM_PATH_CAPACITY)) {
        return false;
    }
    return GetFileAttributesW(wide_path) != INVALID_FILE_ATTRIBUTES;
}

bool platform_file_size(const char *path, uint64_t *size) {
    ASSERT(path != NULL);
    ASSERT(size != NULL);
    wchar_t wide_path[PLATFORM_PATH_CAPACITY];
    if (!platform_utf8_to_utf16(path, wide_path, PLATFORM_PATH_CAPACITY)) {
        return false;
    }
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExW(wide_path, GetFileExInfoStandard, &data) == 0) {
        return false;
    }
    *size = ((uint64_t)data.nFileSizeHigh << 32) | (uint64_t)data.nFileSizeLow;
    return true;
}

bool platform_absolute_path(const char *path, char *buffer, uint64_t capacity) {
    ASSERT(path != NULL);
    ASSERT(buffer != NULL);
    wchar_t wide_path[PLATFORM_PATH_CAPACITY];
    wchar_t full_path[PLATFORM_PATH_CAPACITY];
    if (!platform_utf8_to_utf16(path, wide_path, PLATFORM_PATH_CAPACITY)) {
        return false;
    }
    DWORD length = GetFullPathNameW(wide_path, PLATFORM_PATH_CAPACITY, full_path, NULL);
    if (length == 0 || length >= PLATFORM_PATH_CAPACITY) {
        return false;
    }
    int converted = WideCharToMultiByte(CP_UTF8, 0, full_path, -1, buffer, (int)capacity, NULL, NULL);
    return converted > 0;
}

bool platform_create_directories(const char *path) {
    ASSERT(path != NULL);

    wchar_t buffer[PLATFORM_PATH_CAPACITY];
    if (!platform_utf8_to_utf16(path, buffer, PLATFORM_PATH_CAPACITY)) {
        return false;
    }
    uint64_t length = (uint64_t)wcslen(buffer);
    if (length == 0) {
        return false;
    }

    for (uint64_t index = 1; index < length; index += 1) {
        wchar_t character = buffer[index];
        if (character == L'\\' || character == L'/') {
            buffer[index] = L'\0';
            if (!platform_path_exists_wide(buffer)) {
                if (!CreateDirectoryW(buffer, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                    return false;
                }
            }
            buffer[index] = character;
        }
    }
    if (!platform_path_exists_wide(buffer)) {
        if (!CreateDirectoryW(buffer, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
            return false;
        }
    }
    return true;
}

bool platform_copy_file(const char *from_path, const char *to_path) {
    ASSERT(from_path != NULL);
    ASSERT(to_path != NULL);
    wchar_t wide_from[PLATFORM_PATH_CAPACITY];
    wchar_t wide_to[PLATFORM_PATH_CAPACITY];
    if (!platform_utf8_to_utf16(from_path, wide_from, PLATFORM_PATH_CAPACITY)) {
        return false;
    }
    if (!platform_utf8_to_utf16(to_path, wide_to, PLATFORM_PATH_CAPACITY)) {
        return false;
    }
    return CopyFileW(wide_from, wide_to, FALSE) != 0;
}

bool platform_remove_file(const char *path) {
    ASSERT(path != NULL);
    wchar_t wide_path[PLATFORM_PATH_CAPACITY];
    if (!platform_utf8_to_utf16(path, wide_path, PLATFORM_PATH_CAPACITY)) {
        return false;
    }
    return DeleteFileW(wide_path) != 0;
}

bool platform_remove_directory(const char *path) {
    ASSERT(path != NULL);
    wchar_t wide_path[PLATFORM_PATH_CAPACITY];
    if (!platform_utf8_to_utf16(path, wide_path, PLATFORM_PATH_CAPACITY)) {
        return false;
    }
    return RemoveDirectoryW(wide_path) != 0;
}

uint32_t platform_utf8_arguments(const char **arguments, char *storage, uint64_t storage_capacity) {
    ASSERT(arguments != NULL);
    ASSERT(storage != NULL);

    int wide_count = 0;
    LPWSTR *wide_arguments = CommandLineToArgvW(GetCommandLineW(), &wide_count);
    if (wide_arguments == NULL) {
        return 0;
    }

    uint64_t used = 0;
    uint32_t count = 0;
    bool ok = true;
    for (int index = 0; index < wide_count && ok; index += 1) {
        int required = WideCharToMultiByte(CP_UTF8, 0, wide_arguments[index], -1, NULL, 0, NULL, NULL);
        if (required <= 0 || used + (uint64_t)required > storage_capacity) {
            ok = false;
        } else {
            arguments[count] = storage + used;
            if (WideCharToMultiByte(CP_UTF8, 0, wide_arguments[index], -1, storage + used, required, NULL, NULL) <= 0) {
                ok = false;
            } else {
                used += (uint64_t)required;
                count += 1;
            }
        }
    }
    LocalFree(wide_arguments);
    return ok ? count : 0;
}