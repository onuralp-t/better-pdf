#ifndef BETTER_PDF_SERVER_PROCESS_H
#define BETTER_PDF_SERVER_PROCESS_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Owns the llama-server child process.
 *
 * The server's output goes to <work>/llama-server.log instead of the console,
 * so the program's own log stays readable. Readiness is confirmed by polling
 * the server's /health endpoint: the port accepting connections only proves the
 * process started, not that the model finished loading.
 */

typedef struct {
    void *process;
    void *thread;
} server_process;

bool server_process_start(server_process *server, const char *executable, const char *model_path, const char *mmproj_path, uint16_t port, const char *work_dir, char *error, uint64_t error_capacity);
/* True when anything already answers /health on the port: either a server this
 * run started, or one left behind. The caller must not silently share a server
 * it did not start. */
bool server_process_port_ready(uint16_t port);
bool server_process_wait_ready(const server_process *server, uint16_t port, uint32_t timeout_seconds);
bool server_process_running(const server_process *server);
void server_process_stop(server_process *server);

#endif