#ifndef BETTER_PDF_ARENA_H
#define BETTER_PDF_ARENA_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Bump allocator over caller-provided storage.
 *
 * The whole program allocates exactly once, at startup, sized from the input
 * document and the model's image budget. Every later request is carved out of
 * an arena, so no memory is requested from the operating system after
 * initialization and no individual object needs to be freed.
 *
 * A document arena holds the AST for the whole run. A scratch arena holds the
 * per-page working set (raster, PNG, base64, request, response) and is reset
 * before every page.
 */

typedef struct {
    uint8_t *base;
    uint64_t capacity;
    uint64_t used;
} arena;

void arena_init(arena *arena_value, void *memory, uint64_t capacity);
uint64_t arena_mark(const arena *arena_value);
void arena_reset(arena *arena_value, uint64_t mark);
void *arena_alloc(arena *arena_value, uint64_t size, uint64_t alignment);
uint64_t arena_used(const arena *arena_value);
uint64_t arena_remaining(const arena *arena_value);

#endif