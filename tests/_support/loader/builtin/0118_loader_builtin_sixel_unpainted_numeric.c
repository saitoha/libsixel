/* Distinguish the unpainted sentinel before and after palette expansion. */

#include <string.h>

#include "loader_builtin_memory_test_common.h"

int
test_loader_0118_loader_builtin_sixel_unpainted_numeric(
    int argc,
    char **argv)
{
    unsigned char const payload[] =
        "\033Pq\"1;1;2;1#1;2;100;0;0@\033\\";
    edge_loader_options_t options;
    /* Each probe retains three large raster snapshots. Reuse it after
     * checking indexed delivery instead of doubling the target stack. */
    edge_frame_probe_t probe;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.use_palette = 1;
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    if (edge_load_buffer_options("SIXEL indexed sentinel",
                                 payload,
                                 sizeof(payload) - 1u,
                                 &options,
                                 &probe,
                                 &status) != 0 || SIXEL_FAILED(status)) {
        return 1;
    }
    if (probe.callback_count != 1 || probe.width[0] != 2 ||
        probe.height[0] != 1 ||
        probe.pixelformat[0] != SIXEL_PIXELFORMAT_PAL8 ||
        probe.colorspace[0] != SIXEL_COLORSPACE_GAMMA ||
        probe.ncolors[0] != 2 || probe.rgb_size[0] != 2u ||
        probe.rgb[0][0] != 1u || probe.rgb[0][1] != 255u ||
        probe.palette_size[0] != 6u ||
        probe.palette[0][3] != 255u ||
        probe.palette[0][4] != 0u ||
        probe.palette[0][5] != 0u) {
        return 1;
    }
    options.use_palette = 0;
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    if (edge_load_buffer_options("SIXEL expanded sentinel",
                                 payload,
                                 sizeof(payload) - 1u,
                                 &options,
                                 &probe,
                                 &status) != 0 || SIXEL_FAILED(status)) {
        return 1;
    }
    if (probe.callback_count != 1 || probe.width[0] != 2 ||
        probe.height[0] != 1 ||
        probe.pixelformat[0] != SIXEL_PIXELFORMAT_RGB888 ||
        probe.colorspace[0] != SIXEL_COLORSPACE_GAMMA ||
        probe.rgb_size[0] != 6u ||
        probe.rgb[0][0] != 255u || probe.rgb[0][1] != 0u ||
        probe.rgb[0][2] != 0u || probe.rgb[0][3] != 253u ||
        probe.rgb[0][4] != 253u || probe.rgb[0][5] != 253u) {
        return 1;
    }
    return 0;
}
