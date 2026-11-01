/* Verify CgBI raw-deflate BGRA normalization with an exact pixel. */

#include <string.h>

#include "loader_builtin_memory_test_common.h"

static void
png0157_put_u32be(edge_writer_t *writer, unsigned int value)
{
    edge_put_u8(writer, (value >> 24) & 0xffu);
    edge_put_u8(writer, (value >> 16) & 0xffu);
    edge_put_u8(writer, (value >> 8) & 0xffu);
    edge_put_u8(writer, value & 0xffu);
}

static void
png0157_put_chunk(edge_writer_t *writer,
                  unsigned char const type[4],
                  unsigned char const *payload,
                  size_t payload_size)
{
    static unsigned char const zero_crc[4] = { 0u, 0u, 0u, 0u };

    png0157_put_u32be(writer, (unsigned int)payload_size);
    edge_put_bytes(writer, type, 4u);
    if (payload_size != 0u) {
        edge_put_bytes(writer, payload, payload_size);
    }
    edge_put_bytes(writer, zero_crc, sizeof(zero_crc));
}

int
test_loader_0157_loader_builtin_png_cgbi_numeric(int argc, char **argv)
{
    static unsigned char const signature[8] = {
        0x89u, 'P', 'N', 'G', 0x0du, 0x0au, 0x1au, 0x0au
    };
    static unsigned char const cgbi_type[4] = { 'C', 'g', 'B', 'I' };
    static unsigned char const ihdr_type[4] = { 'I', 'H', 'D', 'R' };
    static unsigned char const idat_type[4] = { 'I', 'D', 'A', 'T' };
    static unsigned char const iend_type[4] = { 'I', 'E', 'N', 'D' };
    static unsigned char const ihdr[13] = {
        0u, 0u, 0u, 1u,
        0u, 0u, 0u, 1u,
        8u, 6u, 0u, 0u, 0u
    };
    static unsigned char const raw_deflate[10] = {
        0x01u, 0x05u, 0x00u, 0xfau, 0xffu,
        0x00u, 25u, 50u, 100u, 128u
    };
    static unsigned char const expected_rgb[3] = { 199u, 100u, 50u };
    unsigned char png[96];
    edge_writer_t writer;
    edge_frame_probe_t probe;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    memset(png, 0, sizeof(png));
    writer.buffer = png;
    writer.capacity = sizeof(png);
    writer.length = 0u;
    writer.failed = 0;
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;

    edge_put_bytes(&writer, signature, sizeof(signature));
    png0157_put_chunk(&writer, cgbi_type, NULL, 0u);
    png0157_put_chunk(&writer, ihdr_type, ihdr, sizeof(ihdr));
    png0157_put_chunk(&writer, idat_type, raw_deflate, sizeof(raw_deflate));
    png0157_put_chunk(&writer, iend_type, NULL, 0u);
    if (writer.failed != 0 ||
        edge_load_buffer("PNG CgBI",
                         png,
                         writer.length,
                         1,
                         &probe,
                         &status) != 0 ||
        SIXEL_FAILED(status) ||
        probe.callback_count != 1 ||
        probe.width[0] != 1 || probe.height[0] != 1 ||
        probe.pixelformat[0] != SIXEL_PIXELFORMAT_RGB888 ||
        probe.colorspace[0] != SIXEL_COLORSPACE_GAMMA ||
        probe.rgb_size[0] != sizeof(expected_rgb) ||
        memcmp(probe.rgb[0], expected_rgb, sizeof(expected_rgb)) != 0) {
        return 1;
    }
    return 0;
}
