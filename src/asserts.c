#include "asserts.h"
#include "log.h"

#include <stdlib.h>

[[noreturn]] void assert_fail(const char *file, uint32_t line, const char *expression) {
    ASSERT(file != NULL);
    ASSERT(expression != NULL);
    log_emit(LOG_LEVEL_ERROR, file, line, "assertion failed: %s", expression);
#if defined(BETTER_PDF_RELEASE)
    /* Release builds never call this function; abort keeps it total anyway. */
    abort();
#else
    /* int3: the debugger stops here with the failing expression on the stack.
     * abort() after it only runs if the debugger steps over the trap. */
    __builtin_debugtrap();
    abort();
#endif
}