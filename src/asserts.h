#ifndef BETTER_PDF_ASSERTS_H
#define BETTER_PDF_ASSERTS_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Assertion policy.
 *
 * ASSERT documents and checks preconditions, postconditions and invariants.
 * A failed assertion is always a programming error: it is logged as ERROR with
 * the file, line and expression, and then aborts the process so the state can
 * be inspected in a debugger.
 *
 * ASSERT must only be used on expressions without side effects: coverage
 * builds compile the expression but do not evaluate it, so that the guards of
 * correct programs do not appear as uncovered branches.
 *
 * In coverage builds, expressions evaluated only by ASSERT would no longer be
 * evaluated, which would change program behaviour. Never wrap a call with a
 * side effect in ASSERT; assign its result to a local first.
 */

[[noreturn]] void assert_fail(const char *file, uint32_t line, const char *expression);

#if defined(BETTER_PDF_COVERAGE_BUILD)
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