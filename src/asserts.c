#include "asserts.h"
#include "log.h"

#include <stdlib.h>

[[noreturn]] void assert_fail(const char *file, uint32_t line, const char *expression) {
    ASSERT(file != NULL);
    ASSERT(expression != NULL);
    log_emit(LOG_LEVEL_ERROR, file, line, "assertion failed: %s", expression);
    abort();
}