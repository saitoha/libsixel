/* SPDX-License-Identifier: MIT
 * Hidden nonfinite storage is opaque. Visible invalid triplets must fail
 * without publication, and a short mask covers only its own samples. */
#include "../../_support/processing/gamut/gamut_test_common.h"

int
test_gamut_0061_float32_hidden_nonfinite(int argc, char **argv)
{
    int const policies[] = {SIXEL_GAMUT_LOCAL_MINDE,
        SIXEL_GAMUT_EDGE_SEEKER, SIXEL_GAMUT_RAY_TRACE};
    int const spaces[] = {SIXEL_COLORSPACE_LINEAR, SIXEL_COLORSPACE_OKLAB};
    unsigned char hidden[] = {1, 0};
    gamut_fixture_t fixture = {0};
    float pixels[6];
    float original[6];
    int policy, space, bad, ok;

    (void)argc;
    (void)argv;
    ok = SIXEL_SUCCEEDED(sixel_allocator_new(&fixture.allocator,
        NULL, NULL, NULL, NULL));
    for (policy = 0; ok && policy < 3; ++policy) {
        ok = SIXEL_SUCCEEDED(sixel_gamut_new(&fixture.mapper, NULL,
            SIXEL_COLORSPACE_GAMMA, policies[policy], fixture.allocator));
        for (space = 0; ok && space < 2; ++space) {
            for (bad = 0; ok && bad < 2; ++bad) {
                pixels[0] = bad ? INFINITY : NAN;
                pixels[1] = -INFINITY;
                pixels[2] = -0.0f;
                pixels[3] = pixels[4] = pixels[5] = .5f;
                memcpy(original, pixels, sizeof(pixels));
                ok = SIXEL_SUCCEEDED(sixel_gamut_apply_float32(
                    fixture.mapper, pixels, 2, spaces[space], hidden, 1));
                ok = ok && memcmp(original, pixels, 3 * sizeof(float)) == 0;
                ok = ok && SIXEL_FAILED(sixel_gamut_apply_float32(
                    fixture.mapper, pixels, 2, spaces[space], hidden, 0));
                ok = ok && memcmp(original, pixels, 3 * sizeof(float)) == 0;
                /* The second pixel is visible outside the mask extent. */
                pixels[3] = NAN;
                ok = ok && SIXEL_FAILED(sixel_gamut_apply_float32(
                    fixture.mapper, pixels, 2, spaces[space], hidden, 1));
            }
        }
        sixel_gamut_free(fixture.mapper);
        fixture.mapper = NULL;
    }
    gamut_fixture_free(&fixture);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
