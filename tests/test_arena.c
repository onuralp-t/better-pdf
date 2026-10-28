#include "test.h"

#include "arena.h"

#include <string.h>

static void test_arena_alignment(void) {
    _Alignas(64) uint8_t memory[4096];
    arena value;
    arena_init(&value, memory, sizeof memory);
    CHECK_U64_EQ(arena_used(&value), 0);
    CHECK_U64_EQ(arena_remaining(&value), sizeof memory);

    uint8_t *first = (uint8_t *)arena_alloc(&value, 1, 1);
    CHECK(first == memory);
    uint8_t *second = (uint8_t *)arena_alloc(&value, 8, 8);
    CHECK((((uintptr_t)second) % 8) == 0);
    CHECK(second >= first);
    CHECK_U64_EQ(arena_used(&value), (uint64_t)(second - memory) + 8u);

    uint8_t *third = (uint8_t *)arena_alloc(&value, 3, 64);
    CHECK((((uintptr_t)third) % 64) == 0);
}

static void test_arena_mark_and_reset(void) {
    _Alignas(64) uint8_t memory[256];
    arena value;
    arena_init(&value, memory, sizeof memory);
    uint64_t mark = arena_mark(&value);
    uint8_t *first = (uint8_t *)arena_alloc(&value, 100, 16);
    memset(first, 0xAB, 100);
    CHECK_U64_EQ(arena_used(&value), 100u);
    uint64_t inner = arena_mark(&value);
    (void)arena_alloc(&value, 64, 8);
    arena_reset(&value, inner);
    CHECK_U64_EQ(arena_used(&value), inner);
    arena_reset(&value, mark);
    CHECK_U64_EQ(arena_used(&value), 0);
    uint8_t *again = (uint8_t *)arena_alloc(&value, 100, 16);
    CHECK(again == first);
}

static void test_arena_exact_capacity(void) {
    _Alignas(64) uint8_t memory[32];
    arena value;
    arena_init(&value, memory, sizeof memory);
    (void)arena_alloc(&value, 32, 1);
    CHECK_U64_EQ(arena_remaining(&value), 0);
    CHECK_U64_EQ(arena_used(&value), 32);
}

static void test_arena_reset_to_zero_reuses_storage(void) {
    _Alignas(64) uint8_t memory[64];
    arena value;
    arena_init(&value, memory, sizeof memory);
    uint8_t *first = (uint8_t *)arena_alloc(&value, 10, 1);
    arena_reset(&value, 0);
    uint8_t *second = (uint8_t *)arena_alloc(&value, 10, 1);
    CHECK(first == second);
}

void register_arena_tests(void) {
    test_register("arena_alignment", test_arena_alignment);
    test_register("arena_mark_and_reset", test_arena_mark_and_reset);
    test_register("arena_exact_capacity", test_arena_exact_capacity);
    test_register("arena_reset_to_zero_reuses_storage", test_arena_reset_to_zero_reuses_storage);
}