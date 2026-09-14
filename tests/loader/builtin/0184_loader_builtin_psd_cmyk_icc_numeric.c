/* SPDX-License-Identifier: MIT */
/* Verify original inverted CMYK planes through a complete independent ICC. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "src/cms.h"
#include "loader_builtin_memory_test_common.h"
#include "tests/data/color-numeric/cmyk.h"

int
test_loader_0184_psd_cmyk_icc_numeric(int argc, char **argv)
{
    edge_loader_options_t options;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    float actual[24];
    int result;
    int i;

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    result = edge_load_fixture_options("PSD independent CMYK ICC",
        "/tests/data/color-numeric/cmyk.psd", &options, &probe, &status);
    if (result != 0 || SIXEL_FAILED(status) || probe.callback_count != 1 ||
        probe.width[0] != 8 || probe.height[0] != 1 ||
        probe.pixelformat[0] != SIXEL_PIXELFORMAT_RGBFLOAT32 ||
        probe.colorspace[0] != SIXEL_COLORSPACE_GAMMA ||
        probe.rgb_size[0] != sizeof(actual)) {
        return 1;
    }
    memcpy(actual, probe.rgb[0], sizeof(actual));
    for (i = 0; i < 24; ++i) {
        if (!isfinite(actual[i]) ||
            fabs(actual[i] - color_cmyk_gamma[i / 3][i % 3]) > .0000005) {
            fprintf(stderr, "PSD pixel=%d channel=%d %.9g != %.9g\n",
                i / 3, i % 3, actual[i], color_cmyk_gamma[i / 3][i % 3]);
            return 1;
        }
    }
    return 0;
}
