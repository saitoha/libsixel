/* Verify that an acTL after the first IDAT remains a static PNG chunk. */

#include <string.h>

#include "loader_builtin_memory_test_common.h"

int
test_loader_0177_png_post_idat_actl_static(
    int argc,
    char **argv)
{
    static unsigned char const actl_chunk[20] = {
        0u, 0u, 0u, 8u, 'a', 'c', 'T', 'L',
        0u, 0u, 0u, 1u, 0u, 0u, 0u, 0u,
        0xb4u, 0x2du, 0xe9u, 0xa0u
    };
    unsigned char source[EDGE_BUFFER_CAPACITY];
    unsigned char buffer[EDGE_BUFFER_CAPACITY];
    edge_writer_t writer;
    edge_loader_options_t options;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    size_t source_size;
    size_t offset;
    size_t iend_offset;
    uint32_t chunk_length;
    int seen_idat;
    int result;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_loader_options_init(&options);
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    source_size = 0u;
    offset = 8u;
    iend_offset = 0u;
    chunk_length = 0u;
    seen_idat = 0;
    result = edge_read_fixture(
        "/tests/data/inputs/formats/libpng-minimal-1x1-rgba.png",
        source,
        sizeof(source),
        &source_size);
    if (result != 0) {
        return 1;
    }
    while (offset + 12u <= source_size) {
        chunk_length = ((uint32_t)source[offset + 0u] << 24)
                     | ((uint32_t)source[offset + 1u] << 16)
                     | ((uint32_t)source[offset + 2u] << 8)
                     | (uint32_t)source[offset + 3u];
        if ((size_t)chunk_length > source_size - offset - 12u) {
            return 1;
        }
        if (memcmp(source + offset + 4u, "IDAT", 4u) == 0) {
            seen_idat = 1;
        }
        if (memcmp(source + offset + 4u, "IEND", 4u) == 0) {
            iend_offset = offset;
            break;
        }
        offset += (size_t)chunk_length + 12u;
    }
    if (seen_idat == 0 || iend_offset == 0u) {
        return 1;
    }
    edge_put_bytes(&writer, source, iend_offset);
    edge_put_bytes(&writer, actl_chunk, sizeof(actl_chunk));
    edge_put_bytes(&writer,
                   source + iend_offset,
                   source_size - iend_offset);
    options.set_start_frame_no = 1;
    options.start_frame_no = 1;
    result = edge_load_buffer_options(
        "post-IDAT acTL static classification",
        buffer,
        writer.length,
        &options,
        &probe,
        &status);
    if (writer.failed != 0 || result != 0 || status != SIXEL_OK ||
        probe.callback_count != 1 || probe.width[0] != 1 ||
        probe.height[0] != 1) {
        return 1;
    }
    return 0;
}
