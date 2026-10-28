#include "png.h"

#include "asserts.h"

#define PNG_SIGNATURE_BYTES 8u
#define PNG_IHDR_CHUNK_BYTES 25u
#define PNG_IDAT_OVERHEAD_BYTES 12u
#define PNG_IEND_CHUNK_BYTES 12u
#define PNG_STORED_BLOCK_MAX 65535u
#define PNG_ZLIB_HEADER_BYTES 2u
#define PNG_ADLER_BYTES 4u

static uint32_t png_bytes_per_pixel(png_mode mode) {
    ASSERT(mode == PNG_MODE_GRAY || mode == PNG_MODE_RGB);
    return mode == PNG_MODE_GRAY ? 1u : 3u;
}

static uint64_t png_filtered_length(uint32_t width, uint32_t height, png_mode mode) {
    uint64_t row_bytes = (uint64_t)width * (uint64_t)png_bytes_per_pixel(mode);
    return (uint64_t)height * (row_bytes + 1u);
}

static uint64_t png_block_count(uint64_t filtered_length) {
    return (filtered_length + PNG_STORED_BLOCK_MAX - 1u) / PNG_STORED_BLOCK_MAX;
}

uint32_t png_crc32(const uint8_t *data, uint64_t length) {
    ASSERT(data != NULL);
    uint32_t crc = 0xFFFFFFFFu;
    for (uint64_t index = 0; index < length; index += 1) {
        crc ^= (uint32_t)data[index];
        for (uint32_t bit = 0; bit < 8u; bit += 1) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

static uint32_t png_adler32_update(uint32_t adler, const uint8_t *data, uint64_t length) {
    uint32_t low = adler & 0xFFFFu;
    uint32_t high = (adler >> 16) & 0xFFFFu;
    for (uint64_t index = 0; index < length; index += 1) {
        low = (low + (uint32_t)data[index]) % 65521u;
        high = (high + low) % 65521u;
    }
    return (high << 16) | low;
}

uint32_t png_adler32(const uint8_t *data, uint64_t length) {
    ASSERT(data != NULL);
    return png_adler32_update(1u, data, length);
}

uint64_t png_encoded_capacity(uint32_t width, uint32_t height, png_mode mode) {
    ASSERT(width > 0);
    ASSERT(height > 0);
    ASSERT(mode == PNG_MODE_GRAY || mode == PNG_MODE_RGB);

    uint64_t filtered_length = png_filtered_length(width, height, mode);
    uint64_t zlib_length = PNG_ZLIB_HEADER_BYTES + filtered_length +
                           png_block_count(filtered_length) * 5u + PNG_ADLER_BYTES;
    return PNG_SIGNATURE_BYTES + PNG_IHDR_CHUNK_BYTES + PNG_IDAT_OVERHEAD_BYTES + zlib_length + PNG_IEND_CHUNK_BYTES + 16u;
}

static void png_write_u32_big_endian(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)((value >> 24) & 0xFFu);
    target[1] = (uint8_t)((value >> 16) & 0xFFu);
    target[2] = (uint8_t)((value >> 8) & 0xFFu);
    target[3] = (uint8_t)(value & 0xFFu);
}

static uint64_t png_write_chunk(uint8_t *output, uint64_t offset, const char *type, const uint8_t *data, uint32_t length) {
    png_write_u32_big_endian(output + offset, length);
    offset += 4u;
    for (uint32_t index = 0; index < 4u; index += 1) {
        output[offset + index] = (uint8_t)type[index];
    }
    offset += 4u;
    for (uint32_t index = 0; index < length; index += 1) {
        output[offset + index] = data[index];
    }
    uint32_t crc = png_crc32(output + offset - 4u, (uint64_t)length + 4u);
    offset += length;
    png_write_u32_big_endian(output + offset, crc);
    return offset + 4u;
}

/* Copies [begin, end) of the row-filtered stream. Filter byte 0 is prepended
 * to every row; the pixel rows themselves are borrowed from the raster, which
 * avoids materializing a second copy of the image. */
static void png_copy_filtered_range(const uint8_t *pixels, uint64_t row_bytes, uint64_t begin, uint64_t end, uint8_t *output) {
    uint64_t filtered_row_bytes = row_bytes + 1u;
    uint64_t offset = begin;
    while (offset < end) {
        uint64_t row = offset / filtered_row_bytes;
        uint64_t column = offset % filtered_row_bytes;
        uint64_t available = filtered_row_bytes - column;
        uint64_t take = end - offset < available ? end - offset : available;
        if (column == 0u) {
            output[0] = 0u;
            if (take > 1u) {
                for (uint64_t index = 0; index < take - 1u; index += 1) {
                    output[1u + index] = pixels[row * row_bytes + index];
                }
            }
        } else {
            for (uint64_t index = 0; index < take; index += 1) {
                output[index] = pixels[row * row_bytes + (column - 1u) + index];
            }
        }
        output += take;
        offset += take;
    }
}

uint64_t png_encode(const uint8_t *pixels, uint32_t width, uint32_t height, png_mode mode, uint8_t *output, uint64_t output_capacity) {
    ASSERT(pixels != NULL);
    ASSERT(output != NULL);
    ASSERT(width > 0);
    ASSERT(height > 0);

    uint64_t capacity = png_encoded_capacity(width, height, mode);
    if (capacity > output_capacity) {
        return 0;
    }

    uint64_t row_bytes = (uint64_t)width * (uint64_t)png_bytes_per_pixel(mode);
    uint64_t filtered_length = png_filtered_length(width, height, mode);
    uint64_t blocks = png_block_count(filtered_length);
    uint64_t zlib_length = PNG_ZLIB_HEADER_BYTES + filtered_length + blocks * 5u + PNG_ADLER_BYTES;

    static const uint8_t signature[8] = { 0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au };
    for (uint32_t index = 0; index < 8u; index += 1) {
        output[index] = signature[index];
    }

    uint8_t header[13];
    png_write_u32_big_endian(header, width);
    png_write_u32_big_endian(header + 4u, height);
    header[8] = 8u;
    header[9] = (uint8_t)mode;
    header[10] = 0u;
    header[11] = 0u;
    header[12] = 0u;
    uint64_t offset = png_write_chunk(output, PNG_SIGNATURE_BYTES, "IHDR", header, 13u);

    uint8_t *zlib = output + offset + 8u;
    zlib[0] = 0x78u;
    zlib[1] = 0x01u;
    uint64_t position = 2u;
    uint64_t source_offset = 0u;
    uint32_t adler = 1u;
    for (uint64_t block = 0; block < blocks; block += 1) {
        uint64_t remaining = filtered_length - source_offset;
        uint32_t block_length = (uint32_t)(remaining > PNG_STORED_BLOCK_MAX ? PNG_STORED_BLOCK_MAX : remaining);
        uint32_t final = remaining == (uint64_t)block_length ? 1u : 0u;
        zlib[position] = (uint8_t)final;
        zlib[position + 1u] = (uint8_t)(block_length & 0xFFu);
        zlib[position + 2u] = (uint8_t)((block_length >> 8) & 0xFFu);
        uint32_t complement = ~block_length;
        zlib[position + 3u] = (uint8_t)(complement & 0xFFu);
        zlib[position + 4u] = (uint8_t)((complement >> 8) & 0xFFu);
        position += 5u;
        png_copy_filtered_range(pixels, row_bytes, source_offset, source_offset + (uint64_t)block_length, zlib + position);
        adler = png_adler32_update(adler, zlib + position, block_length);
        position += (uint64_t)block_length;
        source_offset += (uint64_t)block_length;
    }

    zlib[position] = (uint8_t)((adler >> 24) & 0xFFu);
    zlib[position + 1u] = (uint8_t)((adler >> 16) & 0xFFu);
    zlib[position + 2u] = (uint8_t)((adler >> 8) & 0xFFu);
    zlib[position + 3u] = (uint8_t)(adler & 0xFFu);
    position += 4u;
    ASSERT(position == zlib_length);

    offset = png_write_chunk(output, offset, "IDAT", zlib, (uint32_t)zlib_length);
    offset = png_write_chunk(output, offset, "IEND", NULL, 0u);
    ASSERT(offset <= capacity);
    return offset;
}