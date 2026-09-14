/* SPDX-License-Identifier: MIT */
/* Decode an ICC LUT's encoded PCS before writing extended linear RGB. */
#include <math.h>
#include <stdlib.h>
#include "src/cms.h"
#include "tests/data/color-numeric/forward-lut.h"

int
test_cms_0020(int argc, char **argv)
{
    /* The fixture's constant LUT is physical XYZ D50 (0.25, 0.5, 0.75).
     * Decimal Bradford/matrix evaluation gives these linear sRGB values. */
    static double const expected[3] = {
        -0.39307843644472057, 0.7384529717091166, 0.9578093448962933
    };
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *profile;
    float pixels[3];
    size_t channel;
    int ok;

    (void)argc;
    (void)argv;
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    profile = sixel_cms_open_profile_from_mem(color_forward_lut,
                                               sizeof(color_forward_lut));
    pixels[0] = 0.7f;
    pixels[1] = 0.8f;
    pixels[2] = 0.9f;
    ok = sixel_cms_convert_profile_to_linearrgb((unsigned char *)pixels,
        1, 1, SIXEL_PIXELFORMAT_RGBFLOAT32, profile);
    for (channel = 0; ok && channel < 3; ++channel) {
        ok = isfinite(pixels[channel]) &&
            fabs(pixels[channel] - expected[channel]) < 0.0000005;
    }
    sixel_cms_close_profile(profile);
    sixel_cms_set_engine(previous);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
