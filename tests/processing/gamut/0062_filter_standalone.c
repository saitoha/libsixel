/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "gamut_filter_common.h"

int
test_gamut_0062_filter_standalone(int argc, char **argv)
{
    gamut_filter_fixture_t f = {0};
    sixel_frame_interface_t *object;
    sixel_frame_transparency_t mask;
    float pixels[9] = {NAN, -INFINITY, -0.0f, 1.2f, .4f, -.1f, .2f, .3f, .4f};
    float expected[9];
    unsigned char hidden[1] = {1};
    int ok;

    (void)argc;
    (void)argv;
    memcpy(expected, pixels, sizeof(pixels));
    ok = gamut_filter_init(&f, pixels, 3,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, SIXEL_GAMUT_RAY_TRACE);
    if (ok) {
        object = (sixel_frame_interface_t *)f.frame;
        memset(&mask, 0, sizeof(mask));
        mask.transparent = -1;
        mask.transparent_mask = sixel_allocator_malloc(f.allocator, 1);
        if (mask.transparent_mask == NULL) {
            ok = 0;
        } else {
            mask.transparent_mask[0] = 1;
            mask.transparent_mask_size = 1;
            ok = SIXEL_SUCCEEDED(object->vtbl->set_transparency(object,
                &mask));
        }
        ok = ok && SIXEL_SUCCEEDED(sixel_gamut_apply_float32(f.mapper,
            expected, 3, SIXEL_COLORSPACE_LINEAR, hidden, 1));
        ok = ok && SIXEL_SUCCEEDED(sixel_filter_run(f.gamut,
            f.allocator, NULL));
        ok = ok && !memcmp(pixels, expected, sizeof(pixels)) &&
            sixel_frame_get_colorspace(f.frame) == SIXEL_COLORSPACE_LINEAR;
    }
    gamut_filter_free(&f);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
