/* SPDX-License-Identifier: MIT */
/* Preparing a native source cannot make a foreign destination compatible. */
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/rgb-power.h"

int
test_cms_0129(int argc, char **argv)
{
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *profile;
    float pixels[6] = {0.5f, 0.25f, 0.75f, 0.125f, 0.0625f, 1.0f};
    float original[6];
    int ok;

    (void)argc;
    (void)argv;
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_COLORSYNC);
    if (sixel_cms_get_engine() != SIXEL_CMS_ENGINE_COLORSYNC) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    profile = sixel_cms_open_profile_from_mem(
        color_rgb_power_profiles[12].bytes,
        color_rgb_power_profiles[12].length);
    memcpy(original, pixels, sizeof(pixels));
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    ok = profile != NULL && !sixel_cms_convert_profile_to_linearrgb(
        (unsigned char *)pixels, 2, 1, SIXEL_PIXELFORMAT_RGBFLOAT32, profile);
    ok = ok && memcmp(original, pixels, sizeof(pixels)) == 0 &&
        sixel_cms_get_engine() == SIXEL_CMS_ENGINE_BUILTIN;
    sixel_cms_close_profile(profile);
    sixel_cms_set_engine(previous);
    return ok ? 0 : 1;
}
