#include "ocr_client.h"

#include "asserts.h"
#include "http_message.h"
#include "json.h"

#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#define OCR_CONNECT_TIMEOUT 600000u
#define OCR_RECEIVE_TIMEOUT 600000u

static bool ocr_send_all(SOCKET socket, const char *data, uint64_t length) {
    ASSERT(data != NULL);
    uint64_t sent = 0;
    while (sent < length) {
        int result = send(socket, data + sent, (int)(length - sent), 0);
        if (result <= 0) {
            return false;
        }
        sent += (uint64_t)result;
    }
    return true;
}

bool ocr_endpoint_parse(const char *url, ocr_endpoint *endpoint, char *error, uint64_t error_capacity) {
    ASSERT(url != NULL);
    ASSERT(endpoint != NULL);
    ASSERT(error != NULL);

    if (strncmp(url, "http://", 7u) != 0) {
        (void)snprintf(error, (size_t)error_capacity, "server URL must start with http://");
        return false;
    }
    const char *cursor = url + 7u;
    const char *host_end = cursor;
    while (*host_end != '\0' && *host_end != ':' && *host_end != '/') {
        host_end += 1;
    }
    uint64_t host_length = (uint64_t)(host_end - cursor);
    if (host_length == 0u || host_length >= sizeof endpoint->host) {
        (void)snprintf(error, (size_t)error_capacity, "server URL has no usable host");
        return false;
    }
    memcpy(endpoint->host, cursor, (size_t)host_length);
    endpoint->host[host_length] = '\0';

    endpoint->port = 80u;
    if (*host_end == ':') {
        const char *port_begin = host_end + 1;
        uint32_t port = 0;
        const char *port_end = port_begin;
        while (*port_end >= '0' && *port_end <= '9') {
            port = port * 10u + (uint32_t)(*port_end - '0');
            port_end += 1;
        }
        if (port_end == port_begin || port == 0u || port > 65535u) {
            (void)snprintf(error, (size_t)error_capacity, "server URL has an invalid port");
            return false;
        }
        endpoint->port = (uint16_t)port;
        host_end = port_end;
    }
    if (*host_end == '\0') {
        memcpy(endpoint->path, "/", 2u);
    } else if (*host_end == '/') {
        uint64_t path_length = (uint64_t)strlen(host_end);
        if (path_length >= sizeof endpoint->path) {
            (void)snprintf(error, (size_t)error_capacity, "server URL path is too long");
            return false;
        }
        memcpy(endpoint->path, host_end, (size_t)path_length + 1u);
    } else {
        (void)snprintf(error, (size_t)error_capacity, "server URL has trailing garbage");
        return false;
    }
    return true;
}

bool ocr_client_post(const ocr_endpoint *endpoint, const char *body, uint64_t body_length, char *response, uint64_t response_capacity, uint64_t *response_length, uint64_t *body_offset, uint64_t *body_length_out, char *error, uint64_t error_capacity) {
    ASSERT(endpoint != NULL);
    ASSERT(body != NULL);
    ASSERT(response != NULL);

    char host_header[96];
    (void)snprintf(host_header, sizeof host_header, "%s:%u", endpoint->host, (unsigned)endpoint->port);
    char path[160];
    int path_written = snprintf(path, sizeof path, "%s/chat/completions", endpoint->path);
    if (path_written < 0 || (uint64_t)path_written >= sizeof path) {
        (void)snprintf(error, (size_t)error_capacity, "server URL path is too long");
        return false;
    }

    char head[512];
    int head_written = snprintf(head, sizeof head,
        "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %llu\r\nConnection: close\r\n\r\n",
        path, host_header, (unsigned long long)body_length);
    if (head_written < 0 || (uint64_t)head_written >= sizeof head) {
        (void)snprintf(error, (size_t)error_capacity, "request header is too long");
        return false;
    }

    SOCKET connection = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connection == INVALID_SOCKET) {
        (void)snprintf(error, (size_t)error_capacity, "could not create a socket (error %d)", WSAGetLastError());
        return false;
    }
    struct sockaddr_in address;
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint->port);
    if (InetPtonA(AF_INET, endpoint->host, &address.sin_addr) != 1) {
        (void)snprintf(error, (size_t)error_capacity, "server host %s is not an IPv4 address", endpoint->host);
        closesocket(connection);
        return false;
    }
    DWORD timeout = OCR_CONNECT_TIMEOUT;
    (void)setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof timeout);
    (void)setsockopt(connection, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof timeout);

    if (connect(connection, (const struct sockaddr *)&address, sizeof address) != 0) {
        (void)snprintf(error, (size_t)error_capacity, "could not connect to %s:%u (error %d)", endpoint->host, (unsigned)endpoint->port, WSAGetLastError());
        closesocket(connection);
        return false;
    }
    if (!ocr_send_all(connection, head, (uint64_t)head_written) || !ocr_send_all(connection, body, body_length)) {
        (void)snprintf(error, (size_t)error_capacity, "could not send the OCR request (error %d)", WSAGetLastError());
        closesocket(connection);
        return false;
    }

    uint64_t total = 0;
    bool complete = false;
    while (!complete) {
        if (total >= response_capacity) {
            (void)snprintf(error, (size_t)error_capacity, "OCR response is larger than %llu bytes", (unsigned long long)response_capacity);
            break;
        }
        int received = recv(connection, response + total, (int)(response_capacity - total), 0);
        if (received < 0) {
            (void)snprintf(error, (size_t)error_capacity, "could not read the OCR response (error %d)", WSAGetLastError());
            break;
        }
        if (received == 0) {
            complete = true;
            break;
        }
        total += (uint64_t)received;
    }
    closesocket(connection);
    if (!complete) {
        return false;
    }
    if (total == 0u) {
        (void)snprintf(error, (size_t)error_capacity, "OCR server closed the connection without a response");
        return false;
    }

    http_response_head parsed;
    if (!http_response_head_parse((const uint8_t *)response, total, &parsed)) {
        (void)snprintf(error, (size_t)error_capacity, "OCR server sent a malformed HTTP response");
        return false;
    }
    if (parsed.status_code != 200) {
        (void)snprintf(error, (size_t)error_capacity, "OCR server answered HTTP %d", (int)parsed.status_code);
        return false;
    }

    *response_length = total;
    if (parsed.chunked) {
        uint64_t written = 0;
        if (!http_chunked_decode((const uint8_t *)response + parsed.header_length, total - parsed.header_length, (uint8_t *)response, response_capacity, &written)) {
            (void)snprintf(error, (size_t)error_capacity, "OCR server sent malformed chunked framing");
            return false;
        }
        *body_offset = 0;
        *body_length_out = written;
        return true;
    }
    uint64_t available = total - parsed.header_length;
    if (parsed.has_content_length && available < parsed.content_length) {
        (void)snprintf(error, (size_t)error_capacity, "OCR response ended before its Content-Length");
        return false;
    }
    *body_offset = parsed.header_length;
    *body_length_out = parsed.has_content_length ? parsed.content_length : available;
    return true;
}

bool ocr_response_text(const char *body, uint64_t body_length, char *text, uint64_t text_capacity, uint64_t *text_length, char *error, uint64_t error_capacity) {
    ASSERT(body != NULL);
    ASSERT(text != NULL);

    json_value root;
    if (!json_parse_document(body, body_length, &root)) {
        (void)snprintf(error, (size_t)error_capacity, "OCR server sent JSON that could not be parsed");
        return false;
    }
    json_value error_value;
    if (json_object_get(body, body_length, root, "error", &error_value)) {
        char message[256];
        uint64_t message_length = 0;
        if (json_string_copy(body, body_length, error_value, message, sizeof message, &message_length)) {
            (void)snprintf(error, (size_t)error_capacity, "OCR server reported: %s", message);
        } else {
            (void)snprintf(error, (size_t)error_capacity, "OCR server reported an error");
        }
        return false;
    }

    json_value choices;
    if (!json_object_get(body, body_length, root, "choices", &choices) || choices.kind != JSON_KIND_ARRAY) {
        (void)snprintf(error, (size_t)error_capacity, "OCR response has no choices");
        return false;
    }
    json_value choice;
    if (!json_array_get(body, body_length, choices, 0u, &choice)) {
        (void)snprintf(error, (size_t)error_capacity, "OCR response has an empty choices array");
        return false;
    }
    json_value message;
    if (!json_object_get(body, body_length, choice, "message", &message)) {
        (void)snprintf(error, (size_t)error_capacity, "OCR response choice has no message");
        return false;
    }
    json_value content;
    if (!json_object_get(body, body_length, message, "content", &content) || content.kind != JSON_KIND_STRING) {
        (void)snprintf(error, (size_t)error_capacity, "OCR response message has no text");
        return false;
    }
    if (!json_string_copy(body, body_length, content, text, text_capacity, text_length)) {
        (void)snprintf(error, (size_t)error_capacity, "OCR text does not fit into %llu bytes", (unsigned long long)text_capacity);
        return false;
    }
    return true;
}