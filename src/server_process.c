#include "server_process.h"

#include "asserts.h"
#include "platform_win32.h"

#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

#define SERVER_POLL_INTERVAL_MS 250u

bool server_process_port_ready(uint16_t port) {
    SOCKET connection = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connection == INVALID_SOCKET) {
        return false;
    }
    struct sockaddr_in address;
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(0x7f000001u);
    DWORD timeout = 2000u;
    (void)setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof timeout);
    (void)setsockopt(connection, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof timeout);
    bool healthy = false;
    if (connect(connection, (const struct sockaddr *)&address, sizeof address) == 0) {
        const char request[] = "GET /health HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
        if (send(connection, request, (int)(sizeof request - 1u), 0) == (int)(sizeof request - 1u)) {
            char response[256];
            int received = recv(connection, response, (int)sizeof response, 0);
            if (received > 12 && memcmp(response, "HTTP/1.1 200", 12u) == 0) {
                healthy = true;
            }
        }
    }
    closesocket(connection);
    return healthy;
}

bool server_process_start(server_process *server, const char *executable, const char *model_path, const char *mmproj_path, uint16_t port, const char *work_dir, char *error, uint64_t error_capacity) {
    ASSERT(server != NULL);
    ASSERT(executable != NULL);
    ASSERT(model_path != NULL);
    ASSERT(mmproj_path != NULL);
    ASSERT(work_dir != NULL);

    server->process = NULL;
    server->thread = NULL;
    if (!platform_path_exists(executable)) {
        (void)snprintf(error, (size_t)error_capacity, "llama-server not found at %s", executable);
        return false;
    }
    if (!platform_path_exists(model_path)) {
        (void)snprintf(error, (size_t)error_capacity, "model not found at %s", model_path);
        return false;
    }
    if (!platform_path_exists(mmproj_path)) {
        (void)snprintf(error, (size_t)error_capacity, "vision projector not found at %s", mmproj_path);
        return false;
    }

    char command[2048];
    int written = snprintf(command, sizeof command, "\"%s\" -m \"%s\" --mmproj \"%s\" --host 127.0.0.1 --port %u --ctx-size 8192 --parallel 1 --temp 0",
        executable, model_path, mmproj_path, (unsigned)port);
    if (written < 0 || (uint64_t)written >= sizeof command) {
        (void)snprintf(error, (size_t)error_capacity, "llama-server command line is too long");
        return false;
    }

    char log_path[1024];
    int log_written = snprintf(log_path, sizeof log_path, "%s\\llama-server.log", work_dir);
    if (log_written < 0 || (uint64_t)log_written >= sizeof log_path) {
        (void)snprintf(error, (size_t)error_capacity, "work directory path is too long");
        return false;
    }
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

    BOOL created = CreateProcessA(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, work_dir, &startup, &info);
    CloseHandle(log);
    if (input != INVALID_HANDLE_VALUE) {
        CloseHandle(input);
    }
    if (created == 0) {
        (void)snprintf(error, (size_t)error_capacity, "could not start llama-server (error %lu)", GetLastError());
        return false;
    }
    platform_job_assign(info.hProcess);
    server->process = info.hProcess;
    server->thread = info.hThread;
    return true;
}

bool server_process_wait_ready(const server_process *server, uint16_t port, uint32_t timeout_seconds) {
    ASSERT(server != NULL);
    uint32_t waited_ms = 0;
    while (waited_ms <= timeout_seconds * 1000u) {
        if (server_process_port_ready(port)) {
            return true;
        }
        if (!server_process_running(server)) {
            return false;
        }
        Sleep(SERVER_POLL_INTERVAL_MS);
        waited_ms += SERVER_POLL_INTERVAL_MS;
    }
    return false;
}

bool server_process_running(const server_process *server) {
    ASSERT(server != NULL);
    if (server->process == NULL) {
        return false;
    }
    DWORD exit_code = 0;
    if (GetExitCodeProcess((HANDLE)server->process, &exit_code) == 0) {
        return false;
    }
    return exit_code == STILL_ACTIVE;
}

void server_process_stop(server_process *server) {
    ASSERT(server != NULL);
    if (server->process != NULL) {
        (void)TerminateProcess((HANDLE)server->process, 0);
        (void)WaitForSingleObject((HANDLE)server->process, 10000u);
        CloseHandle((HANDLE)server->process);
        server->process = NULL;
    }
    if (server->thread != NULL) {
        CloseHandle((HANDLE)server->thread);
        server->thread = NULL;
    }
}