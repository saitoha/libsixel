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
    edge_frame_probe_t indexed;
    edge_frame_probe_t expanded;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.use_palette = 1;
    memset(&indexed, 0, sizeof(indexed));
    status = SIXEL_FALSE;
    if (edge_load_buffer_options("SIXEL indexed sentinel",
                                 payload,
                                 sizeof(payload) - 1u,
                                 &options,
                                 &indexed,
                                 &status) != 0 || SIXEL_FAILED(status)) {
        return 1;
    }
    options.use_palette = 0;
    memset(&expanded, 0, sizeof(expanded));
    status = SIXEL_FALSE;
    if (edge_load_buffer_options("SIXEL expanded sentinel",
                                 payload,
                                 sizeof(payload) - 1u,
                                 &options,
                                 &expanded,
                                 &status) != 0 || SIXEL_FAILED(status)) {
        return 1;
    }
    if (indexed.callback_count != 1 || indexed.width[0] != 2 ||
        indexed.height[0] != 1 ||
        indexed.pixelformat[0] != SIXEL_PIXELFORMAT_PAL8 ||
        indexed.colorspace[0] != SIXEL_COLORSPACE_GAMMA ||
        indexed.ncolors[0] != 2 || indexed.rgb_size[0] != 2u ||
        indexed.rgb[0][0] != 1u || indexed.rgb[0][1] != 255u ||
        indexed.palette_size[0] != 6u ||
        indexed.palette[0][3] != 255u ||
        indexed.palette[0][4] != 0u ||
        indexed.palette[0][5] != 0u) {
        return 1;
    }
    if (expanded.callback_count != 1 || expanded.width[0] != 2 ||
        expanded.height[0] != 1 ||
        expanded.pixelformat[0] != SIXEL_PIXELFORMAT_RGB888 ||
        expanded.colorspace[0] != SIXEL_COLORSPACE_GAMMA ||
        expanded.rgb_size[0] != 6u ||
        expanded.rgb[0][0] != 255u || expanded.rgb[0][1] != 0u ||
        expanded.rgb[0][2] != 0u || expanded.rgb[0][3] != 253u ||
        expanded.rgb[0][4] != 253u || expanded.rgb[0][5] != 253u) {
        return 1;
    }
    return 0;
}
