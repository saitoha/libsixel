/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/gamut_test_common.h"

int
test_gamut_0017_palette_continuous(int argc, char **argv)
{
    gamut_fixture_t fixture = {0};
    sixel_dither_t *dither = NULL;
    sixel_palette_float32_entries_request_t request;
    sixel_palette_float32_entries_view_t view;
    float colors[6] = {1.4f, -.1f, .15f, 0, 0, 0};
    int ok;
    int c;

    (void)argc;
    (void)argv;
    request.entries = colors;
    request.colors = 2;
    request.depth = 3 * sizeof(float);
    ok = gamut_fixture_init(&fixture, "srgb") &&
        SIXEL_SUCCEEDED(sixel_dither_new(&dither, 2, fixture.allocator)) &&
        SIXEL_SUCCEEDED(dither->palette->vtbl->init_entries_float32(
            dither->palette, &request)) &&
        SIXEL_SUCCEEDED(sixel_palette_map_gamut(dither->palette,
            fixture.mapper, SIXEL_COLORSPACE_LINEAR,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32)) &&
        SIXEL_SUCCEEDED(sixel_palette_reconcile_bytes(dither->palette,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32)) &&
        SIXEL_SUCCEEDED(dither->palette->vtbl->get_entries_float32(
            dither->palette, &view));
    for (c = 0; ok && c < 3; ++c) {
        ok = isfinite(view.entries[c]) &&
            fabs(view.entries[c] - gamut_expected[0][0][c]) < 3e-5;
    }
    sixel_dither_unref(dither);
    gamut_fixture_free(&fixture);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
