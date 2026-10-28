#ifndef BETTER_PDF_OCR_CLIENT_H
#define BETTER_PDF_OCR_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

/*
 * HTTP client for llama-server's OpenAI-compatible chat endpoint.
 *
 * One request carries one rendered page as a base64 PNG; the reply is a JSON
 * document whose first choice's message content is the OCR text. The text is
 * decoded from JSON here so the rest of the program only ever sees UTF-8.
 */

typedef struct {
    char host[64];
    uint16_t port;
    char path[128];
} ocr_endpoint;

bool ocr_endpoint_parse(const char *url, ocr_endpoint *endpoint, char *error, uint64_t error_capacity);

/* Posts the JSON body and returns the decoded response body. Chunked replies are
 * decoded in place, so the body starts at response + body_offset. */
bool ocr_client_post(const ocr_endpoint *endpoint, const char *body, uint64_t body_length, char *response, uint64_t response_capacity, uint64_t *response_length, uint64_t *body_offset, uint64_t *body_length_out, char *error, uint64_t error_capacity);

bool ocr_response_text(const char *body, uint64_t body_length, char *text, uint64_t text_capacity, uint64_t *text_length, char *error, uint64_t error_capacity);

#endif