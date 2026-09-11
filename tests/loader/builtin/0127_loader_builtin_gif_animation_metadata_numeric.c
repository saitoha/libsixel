/* Verify GIF frame timing and sequence metadata on emitted frames. */

#include <string.h>

#include "loader_builtin_gif_test_common.h"

int
test_loader_0127_loader_builtin_gif_animation_metadata_numeric(
    int argc,
    char **argv)
{
    unsigned char buffer[256];
    unsigned char const red[1] = { 1u };
    unsigned char const green[1] = { 2u };
    edge_writer_t writer;
    edge_frame_probe_t probe;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;

    edge_gif_begin(&writer, 1, 1u, 1u);
    edge_gif_graphic_control_delay(&writer, 0u, 7u);
    edge_gif_image(&writer, 0u, 0u, 1u, 1u, 0, red, 1u, 1);
    edge_gif_graphic_control_delay(&writer, 0u, 23u);
    edge_gif_image(&writer, 0u, 0u, 1u, 1u, 0, green, 1u, 1);
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed != 0 ||
        edge_load_buffer("GIF animation metadata",
                         buffer,
                         writer.length,
                         0,
                         &probe,
                         &status) != 0 ||
        SIXEL_FAILED(status)) {
        return 1;
    }
    if (probe.callback_count != 2 ||
        probe.delay[0] != 7 || probe.delay[1] != 23 ||
        probe.frame_no[0] != 0 || probe.frame_no[1] != 1 ||
        probe.loop_no[0] != 0 || probe.loop_no[1] != 0 ||
        probe.multiframe[0] == 0 || probe.multiframe[1] == 0) {
        return 1;
    }
    return 0;
}
