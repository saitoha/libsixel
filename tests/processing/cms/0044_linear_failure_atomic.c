/* SPDX-License-Identifier: MIT */
/* A late failed apply must not consume the loader's original samples. */
#include <float.h>
#include <string.h>
#include "src/cms.h"

int
test_cms_0044(int argc, char **argv)
{
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *profile;
    float pixels[9] = {0.5f, 0.25f, 0.75f, FLT_MAX, 0.5f, 0.25f,
                       0.75f, 0.5f, 0.25f};
    float original[9];
    int ok;

    (void)argc;
    (void)argv;
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    /* Either traversal direction writes a valid endpoint before the middle
     * sample overflows float output after analytic sRGB transfer decoding. */
    profile = sixel_cms_create_srgb_profile();
    memcpy(original, pixels, sizeof(pixels));
    ok = profile != NULL && !sixel_cms_convert_profile_to_linearrgb(
        (unsigned char *)pixels, 3, 1, SIXEL_PIXELFORMAT_RGBFLOAT32, profile);
    ok = ok && memcmp(original, pixels, sizeof(pixels)) == 0;
    sixel_cms_close_profile(profile);
    sixel_cms_set_engine(previous);
    return ok ? 0 : 1;
}
