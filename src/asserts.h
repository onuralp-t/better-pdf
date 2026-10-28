#ifndef BETTER_PDF_ASSERTS_H
#define BETTER_PDF_ASSERTS_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Assertion policy.
 *
 * ASSERT documents and checks preconditions, postconditions and invariants. A
 * failed assertion is always a programming error: it is logged with the file,
 * line and expression, and then executes an int3 so an attached debugger stops
 * exactly where the invariant broke. Without a debugger the trap terminates the
 * process at that instruction instead of unwinding into unrelated code.
 *
 * Release builds and coverage builds compile the expression but never evaluate
 * it, so a release binary pays nothing for its assertions and the guards of a
 * correct program do not appear as uncovered branches. That also means an
 * expression evaluated only by ASSERT is dead code in those builds: never put a
 * side effect in an assertion, assign the result to a local first.
 */

[[noreturn]] void assert_fail(const char *file, uint32_t line, const char *expression);

#if defined(BETTER_PDF_RELEASE) || defined(BETTER_PDF_COVERAGE_BUILD)
#define ASSERT(condition) ((void)sizeof(condition))
#else
#define ASSERT(condition)                                            \
    do {                                                             \
        if (!(condition)) {                                          \
            assert_fail(__FILE__, (uint32_t)__LINE__, #condition);   \
        }                                                            \
    } while (0)
#endif

#define REQUIRE(condition) ASSERT(condition)

#define STATIC_ASSERT(condition, message) _Static_assert(condition, message)

#endif