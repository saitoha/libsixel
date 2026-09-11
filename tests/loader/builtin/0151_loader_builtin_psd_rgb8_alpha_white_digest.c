/* Fix PSD alpha composition over an explicit white background. */

#include <stdio.h>

#include "loader_builtin_memory_test_common.h"

int
test_loader_0151_loader_builtin_psd_rgb8_alpha_white_digest(int argc,
                                                             char **argv)
{
    edge_loader_options_t options;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    uint64_t digest;
    int result;

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.set_bgcolor = 1;
    options.bgcolor[0] = 255u;
    options.bgcolor[1] = 255u;
    options.bgcolor[2] = 255u;
    status = SIXEL_FALSE;
    digest = 0u;
    result = edge_load_fixture_options(
        "PSD RGB8 alpha over white",
        "/tests/data/inputs/formats/snake16_rgb8_alpha.psd",
        &options,
        &probe,
        &status);
    if (result != 0 || SIXEL_FAILED(status) || probe.callback_count != 1 ||
        probe.width[0] != 16 || probe.height[0] != 16 ||
        probe.pixelformat[0] != SIXEL_PIXELFORMAT_RGB888 ||
        probe.rgb_size[0] != 16u * 16u * 3u || probe.mask_size[0] != 0u) {
        return 1;
    }
    digest = edge_digest_bytes(probe.rgb[0], probe.rgb_size[0]);
    if (digest != UINT64_C(0x4d2c7a157a1ebd63)) {
        fprintf(stderr, "PSD white RGB digest 0x%016llx\n",
                (unsigned long long)digest);
        return 1;
    }
    return 0;
}
