/* SPDX-License-Identifier: MIT */
#include "../../_support/processing/gamut/gamut_filter_common.h"

int
test_gamut_0067_pipeline_mask(int argc, char **argv)
{
    gamut_filter_fixture_t f = {0};
    sixel_frame_interface_t *object;
    sixel_frame_transparency_t mask;
    sixel_thread_budget_scope_t scope;
    float *pixels, *expected;
    size_t i, count;
    int ok;

    (void)argc;
    (void)argv;
    count = 262145;
    pixels = calloc(count * 3, sizeof(float));
    expected = malloc(count * 3 * sizeof(float));
    ok = pixels != NULL && expected != NULL &&
        gamut_filter_init(&f, pixels, count,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, SIXEL_GAMUT_EDGE_SEEKER);
    if (ok) {
        /* The short mask crosses the tile boundary. The tail of the second
         * tile and the entire third tile must become visible again. */
        for (i = 0; i < count; ++i) {
            pixels[i * 3] = i < 131073 ? NAN : 1.2f;
            pixels[i * 3 + 1] = .4f;
            pixels[i * 3 + 2] = -.1f;
        }
        memcpy(expected, pixels, count * 3 * sizeof(float));
        object = (sixel_frame_interface_t *)f.frame;
        memset(&mask, 0, sizeof(mask));
        mask.transparent = -1;
        mask.transparent_mask_size = 131073;
        mask.transparent_mask = sixel_allocator_malloc(f.allocator, 131073);
        ok = mask.transparent_mask != NULL;
        if (ok) {
            memset(mask.transparent_mask, 1, 131073);
            ok = SIXEL_SUCCEEDED(object->vtbl->set_transparency(object,
                &mask)) && SIXEL_SUCCEEDED(sixel_gamut_apply_float32(
                    f.mapper, expected, count, SIXEL_COLORSPACE_LINEAR,
                    mask.transparent_mask, mask.transparent_mask_size));
        }
        ok = ok && gamut_filter_scope(&scope, "balanced:global_budget=4");
        if (ok) {
            ok = sixel_filter_pipeline_run(f.color, f.gamut,
                f.allocator, NULL) == SIXEL_OK &&
                !memcmp(pixels, expected, count * 3 * sizeof(float));
            sixel_thread_budget_leave(&scope);
        }
    }
    gamut_filter_free(&f);
    free(pixels);
    free(expected);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
