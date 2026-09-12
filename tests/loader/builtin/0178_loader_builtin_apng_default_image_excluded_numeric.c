/* Verify that a default PNG image is excluded from APNG frame numbering. */

#include <string.h>

#include "loader_builtin_memory_test_common.h"

int
test_loader_0178_apng_default_excluded(
    int argc,
    char **argv)
{
    unsigned char buffer[EDGE_BUFFER_CAPACITY];
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    size_t buffer_size;
    size_t offset;
    uint32_t chunk_length;
    int patched;
    int result;

    (void)argc;
    (void)argv;
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    buffer_size = 0u;
    offset = 8u;
    chunk_length = 0u;
    patched = 0;
    result = edge_read_fixture(
        "/tests/data/inputs/formats/"
        "apng_8x8_libpng_default_image_first_valid.png",
        buffer,
        sizeof(buffer),
        &buffer_size);
    if (result != 0) {
        return 1;
    }
    while (offset + 12u <= buffer_size) {
        chunk_length = ((uint32_t)buffer[offset + 0u] << 24)
                     | ((uint32_t)buffer[offset + 1u] << 16)
                     | ((uint32_t)buffer[offset + 2u] << 8)
                     | (uint32_t)buffer[offset + 3u];
        if ((size_t)chunk_length > buffer_size - offset - 12u) {
            return 1;
        }
        if (memcmp(buffer + offset + 4u, "acTL", 4u) == 0) {
            if (chunk_length != 8u) {
                return 1;
            }
            buffer[offset + 8u] = 0u;
            buffer[offset + 9u] = 0u;
            buffer[offset + 10u] = 0u;
            buffer[offset + 11u] = 1u;
            buffer[offset + 16u] = 0xb4u;
            buffer[offset + 17u] = 0x2du;
            buffer[offset + 18u] = 0xe9u;
            buffer[offset + 19u] = 0xa0u;
            patched = 1;
            break;
        }
        offset += (size_t)chunk_length + 12u;
    }
    result = edge_load_buffer("APNG excluded default image",
                              buffer,
                              buffer_size,
                              0,
                              &probe,
                              &status);
    if (patched == 0 || result != 0 || status != SIXEL_OK ||
        probe.callback_count != 1 || probe.width[0] != 8 ||
        probe.height[0] != 8 || probe.frame_no[0] != 0) {
        return 1;
    }
    return 0;
}
