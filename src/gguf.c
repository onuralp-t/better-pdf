#include "gguf.h"

#include "asserts.h"

#include <limits.h>
#include <string.h>

#define GGUF_MAGIC 0x46554747u /* "GGUF" read as a little-endian word */
#define GGUF_KEY_CAPACITY 512u

static bool gguf_read_bytes(FILE *file, void *buffer, uint64_t size) {
    if (size == 0u) {
        return true;
    }
    return fread(buffer, 1, (size_t)size, file) == (size_t)size;
}

static bool gguf_read_u32(FILE *file, uint32_t *value) {
    uint8_t bytes[4];
    if (!gguf_read_bytes(file, bytes, 4u)) {
        return false;
    }
    *value = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
    return true;
}

static bool gguf_read_u64(FILE *file, uint64_t *value) {
    uint8_t bytes[8];
    if (!gguf_read_bytes(file, bytes, 8u)) {
        return false;
    }
    uint64_t result = 0;
    for (uint32_t index = 0; index < 8u; index += 1) {
        result |= (uint64_t)bytes[index] << (index * 8u);
    }
    *value = result;
    return true;
}

static bool gguf_skip(FILE *file, uint64_t size) {
    if (size > (uint64_t)INT64_MAX) {
        return false;
    }
#ifdef _WIN32
    return _fseeki64(file, (int64_t)size, SEEK_CUR) == 0;
#else
    if (size > (uint64_t)LONG_MAX) {
        return false;
    }
    return fseek(file, (long)size, SEEK_CUR) == 0;
#endif
}

static bool gguf_read_string(FILE *file, char *output, uint64_t capacity, uint64_t *length) {
    ASSERT(output != NULL);
    ASSERT(length != NULL);

    uint64_t size = 0;
    if (!gguf_read_u64(file, &size)) {
        return false;
    }
    if (size >= capacity) {
        return false;
    }
    if (!gguf_read_bytes(file, output, size)) {
        return false;
    }
    output[size] = '\0';
    *length = size;
    return true;
}

static uint64_t gguf_scalar_size(uint32_t type) {
    switch (type) {
        case 0: /* uint8 */
        case 1: /* int8 */
        case 7: /* bool */
            return 1u;
        case 2: /* uint16 */
        case 3: /* int16 */
            return 2u;
        case 4: /* uint32 */
        case 5: /* int32 */
        case 6: /* float32 */
            return 4u;
        case 10: /* uint64 */
        case 11: /* int64 */
        case 12: /* float64 */
            return 8u;
        default:
            return 0u;
    }
}

static bool gguf_skip_array(FILE *file, uint32_t element_type, uint64_t count) {
    if (element_type == 9u) {
        return false;
    }
    if (element_type == 8u) {
        for (uint64_t index = 0; index < count; index += 1) {
            uint64_t size = 0;
            if (!gguf_read_u64(file, &size)) {
                return false;
            }
            if (!gguf_skip(file, size)) {
                return false;
            }
        }
        return true;
    }
    uint64_t element_size = gguf_scalar_size(element_type);
    if (element_size == 0u) {
        return false;
    }
    if (count > UINT64_MAX / element_size) {
        return false;
    }
    return gguf_skip(file, count * element_size);
}

static bool gguf_skip_value(FILE *file, uint32_t type) {
    if (type == 8u) {
        uint64_t size = 0;
        if (!gguf_read_u64(file, &size)) {
            return false;
        }
        return gguf_skip(file, size);
    }
    if (type == 9u) {
        uint32_t element_type = 0;
        uint64_t count = 0;
        if (!gguf_read_u32(file, &element_type)) {
            return false;
        }
        if (!gguf_read_u64(file, &count)) {
            return false;
        }
        return gguf_skip_array(file, element_type, count);
    }
    uint64_t size = gguf_scalar_size(type);
    if (size == 0u) {
        return false;
    }
    return gguf_skip(file, size);
}

static bool gguf_read_unsigned(FILE *file, uint32_t type, uint64_t *value) {
    uint8_t bytes[8];
    uint64_t size = gguf_scalar_size(type);
    if (size == 0u || type == 6u || type == 12u || !gguf_read_bytes(file, bytes, size)) {
        return false;
    }
    uint64_t result = 0;
    for (uint64_t index = 0; index < size; index += 1) {
        result |= (uint64_t)bytes[index] << (index * 8u);
    }
    if (type == 1u) {
        int8_t signed_value = (int8_t)result;
        if (signed_value < 0) {
            return false;
        }
        result = (uint64_t)signed_value;
    } else if (type == 3u) {
        int16_t signed_value = (int16_t)(uint16_t)result;
        if (signed_value < 0) {
            return false;
        }
        result = (uint64_t)signed_value;
    } else if (type == 5u) {
        int32_t signed_value = (int32_t)(uint32_t)result;
        if (signed_value < 0) {
            return false;
        }
        result = (uint64_t)signed_value;
    } else if (type == 11u) {
        int64_t signed_value = (int64_t)result;
        if (signed_value < 0) {
            return false;
        }
        result = (uint64_t)signed_value;
    }
    *value = result;
    return true;
}

bool gguf_read_metadata_u64(FILE *file, const char *key, uint64_t *value, bool *found) {
    ASSERT(file != NULL);
    ASSERT(key != NULL);
    ASSERT(value != NULL);
    ASSERT(found != NULL);

    *found = false;
    *value = 0;

    uint32_t magic = 0;
    if (!gguf_read_u32(file, &magic) || magic != GGUF_MAGIC) {
        return false;
    }
    uint32_t version = 0;
    if (!gguf_read_u32(file, &version)) {
        return false;
    }
    if (version < 2u || version > 3u) {
        return false;
    }
    uint64_t tensor_count = 0;
    uint64_t entry_count = 0;
    if (!gguf_read_u64(file, &tensor_count) || !gguf_read_u64(file, &entry_count)) {
        return false;
    }
    (void)tensor_count;

    for (uint64_t entry = 0; entry < entry_count; entry += 1) {
        char entry_key[GGUF_KEY_CAPACITY];
        uint64_t key_length = 0;
        if (!gguf_read_string(file, entry_key, GGUF_KEY_CAPACITY, &key_length)) {
            return false;
        }
        uint32_t type = 0;
        if (!gguf_read_u32(file, &type)) {
            return false;
        }
        if (strcmp(entry_key, key) == 0) {
            uint64_t converted = 0;
            if (!gguf_read_unsigned(file, type, &converted)) {
                return false;
            }
            *found = true;
            *value = converted;
            return true;
        }
        if (!gguf_skip_value(file, type)) {
            return false;
        }
    }
    return true;
}