#include "arena.h"

#include "asserts.h"

#define ARENA_BASE_ALIGNMENT 64u

void arena_init(arena *arena_value, void *memory, uint64_t capacity) {
    ASSERT(arena_value != NULL);
    ASSERT(memory != NULL);
    ASSERT(capacity > 0);

    uintptr_t base_address = (uintptr_t)memory;
    uintptr_t aligned_address = (base_address + ARENA_BASE_ALIGNMENT - 1u) & ~(uintptr_t)(ARENA_BASE_ALIGNMENT - 1u);
    uint64_t skipped = (uint64_t)(aligned_address - base_address);
    if (capacity < ARENA_BASE_ALIGNMENT) {
        aligned_address = base_address;
        skipped = 0;
    }
    ASSERT(skipped < capacity);

    arena_value->base = (uint8_t *)aligned_address;
    arena_value->capacity = capacity - skipped;
    arena_value->used = 0;
}

uint64_t arena_mark(const arena *arena_value) {
    ASSERT(arena_value != NULL);
    return arena_value->used;
}

void arena_reset(arena *arena_value, uint64_t mark) {
    ASSERT(arena_value != NULL);
    ASSERT(mark <= arena_value->used);
    arena_value->used = mark;
}

uint64_t arena_used(const arena *arena_value) {
    ASSERT(arena_value != NULL);
    return arena_value->used;
}

uint64_t arena_remaining(const arena *arena_value) {
    ASSERT(arena_value != NULL);
    return arena_value->capacity - arena_value->used;
}

void *arena_alloc(arena *arena_value, uint64_t size, uint64_t alignment) {
    ASSERT(arena_value != NULL);
    ASSERT(size > 0);
    ASSERT(alignment > 0);
    ASSERT(alignment <= ARENA_BASE_ALIGNMENT);
    ASSERT((alignment & (alignment - 1)) == 0);

    uint64_t aligned = (arena_value->used + alignment - 1u) & ~(alignment - 1u);
    ASSERT(aligned <= arena_value->capacity);
    uint64_t end = aligned + size;
    ASSERT(end <= arena_value->capacity);
    ASSERT(end >= aligned);

    arena_value->used = end;
    return arena_value->base + aligned;
}