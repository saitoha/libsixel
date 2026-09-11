/* Shared PNG chunk navigation for exact color-management tests. */

#ifndef LOADER_BUILTIN_PNG_COLOR_TEST_COMMON_H
#define LOADER_BUILTIN_PNG_COLOR_TEST_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint32_t
png_color_read_be32(unsigned char const *bytes)
{
    return ((uint32_t)bytes[0] << 24) |
           ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) |
           (uint32_t)bytes[3];
}

static int
png_color_find_chunk(unsigned char const *png,
                     size_t png_size,
                     char const type[4],
                     size_t *chunk_offset,
                     size_t *chunk_size)
{
    size_t offset;
    size_t payload_size;
    size_t total_size;

    offset = 8u;
    payload_size = 0u;
    total_size = 0u;
    if (png == NULL || type == NULL || chunk_offset == NULL ||
        chunk_size == NULL || png_size < 8u) {
        return 0;
    }
    while (offset <= png_size && png_size - offset >= 12u) {
        payload_size = (size_t)png_color_read_be32(png + offset);
        if (payload_size > png_size - offset - 12u) {
            return 0;
        }
        total_size = payload_size + 12u;
        if (memcmp(png + offset + 4u, type, 4u) == 0) {
            *chunk_offset = offset;
            *chunk_size = total_size;
            return 1;
        }
        offset += total_size;
    }
    return 0;
}

#endif
