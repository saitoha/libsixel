/* SPDX-License-Identifier: MIT */
/* A borrowed source remains reusable after prepared transforms are deleted. */
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/rgb-power.h"

int
test_cms_0128(int argc, char **argv)
{
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *profile;
    unsigned char bytes[1024];
    float pixels[COLOR_RGB_POWER_SAMPLES * 3];
    float first[COLOR_RGB_POWER_SAMPLES * 3];
    int p;
    int c;
    int repeat;
    int ok;

    (void)argc;
    (void)argv;
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_COLORSYNC);
    if (sixel_cms_get_engine() != SIXEL_CMS_ENGINE_COLORSYNC) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    /* mask4 para0 has mixed powers [2,4,1], requiring a paired exchange.
     * The profile must own its bytes before the caller discards them. */
    memcpy(bytes, color_rgb_power_profiles[12].bytes, sizeof(bytes));
    profile = sixel_cms_open_profile_from_mem(bytes,
        color_rgb_power_profiles[12].length);
    memset(bytes, 0xa5, sizeof(bytes));
    ok = profile != NULL;
    for (repeat = 0; ok && repeat < 3; ++repeat) {
        for (p = 0; p < COLOR_RGB_POWER_SAMPLES; ++p) {
            for (c = 0; c < 3; ++c) {
                pixels[p * 3 + c] =
                    (float)(color_rgb_power_source[p][c] / 65535.0);
            }
        }
        ok = sixel_cms_convert_profile_to_linearrgb((unsigned char *)pixels,
            COLOR_RGB_POWER_SAMPLES, 1, SIXEL_PIXELFORMAT_RGBFLOAT32,
            profile);
        if (repeat == 0) {
            memcpy(first, pixels, sizeof(pixels));
        } else {
            ok = ok && memcmp(first, pixels, sizeof(pixels)) == 0;
        }
    }
    sixel_cms_close_profile(profile);
    sixel_cms_set_engine(previous);
    return ok ? 0 : 1;
}
