#ifndef BETTER_PDF_HTTP_MESSAGE_H
#define BETTER_PDF_HTTP_MESSAGE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * HTTP/1.1 framing helpers.
 *
 * The client talks to a local llama-server that answers with either
 * Content-Length or chunked framing, so both are decoded here. The parsing is
 * pure: it works on a byte buffer and reports offsets, which lets the socket
 * layer decide how much more to read.
 */

typedef struct {
    int32_t status_code;
    bool has_content_length;
    uint64_t content_length;
    bool chunked;
    uint64_t header_length;
} http_response_head;

uint64_t http_request_build(char *output, uint64_t output_capacity, const char *host, uint16_t port, const char *path, const char *body, uint64_t body_length);
bool http_response_head_parse(const uint8_t *data, uint64_t length, http_response_head *head);
bool http_chunked_decode(const uint8_t *data, uint64_t length, uint8_t *output, uint64_t output_capacity, uint64_t *written);

#endif