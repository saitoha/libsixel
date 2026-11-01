/* SPDX-License-Identifier: MIT
 * The plane fast path must match scalar arithmetic bit for bit. Exercise
 * destination matrices, source workspaces and both sides of gamut edges. */
#include "../../_support/processing/gamut/gamut_test_common.h"
#include "tests/data/color-numeric/icc.h"

int
test_gamut_0060_float32_scalar_equivalence(int argc, char **argv)
{
    char const *names[] = {"srgb", "display-p3", "adobe-rgb", "dci-p3"};
    int const policies[] = {SIXEL_GAMUT_LOCAL_MINDE,
        SIXEL_GAMUT_EDGE_SEEKER, SIXEL_GAMUT_RAY_TRACE};
    int const spaces[] = {SIXEL_COLORSPACE_LINEAR, SIXEL_COLORSPACE_GAMMA,
        SIXEL_COLORSPACE_OKLAB, SIXEL_COLORSPACE_CIELAB,
        SIXEL_COLORSPACE_DIN99D};
    float const edges[] = {-0.0f, 0.0f, -2e-7f, 2e-7f,
        1.0f, 1.0f + 2e-7f, -1e-30f, 1e-30f};
    gamut_fixture_t fixture = {0};
    float pixels[259 * 3];
    float expected[259 * 3];
    double source[3];
    double input[3];
    double output[3];
    size_t i;
    int destination, policy, space, c, ok;

    (void)argc;
    (void)argv;
    ok = SIXEL_SUCCEEDED(sixel_allocator_new(&fixture.allocator,
        NULL, NULL, NULL, NULL));
    for (destination = 0; ok && destination < 6; ++destination) {
        fixture.definition = destination < 4
            ? sixel_cdef_name(names[destination]) : destination == 5
                ? sixel_cdef_icc(color_icc, sizeof(color_icc)) : NULL;
        ok = destination == 4 || fixture.definition != NULL;
        for (policy = 0; ok && policy < 3; ++policy) {
            ok = SIXEL_SUCCEEDED(sixel_gamut_new(&fixture.mapper,
                fixture.definition, destination == 4
                    ? SIXEL_COLORSPACE_SMPTEC : SIXEL_COLORSPACE_GAMMA,
                policies[policy], fixture.allocator));
            for (space = 0; ok && space < 5; ++space) {
                for (i = 0; ok && i < 259; ++i) {
                    source[0] = (i * 37 % 257) / 256.0;
                    source[1] = (i * 71 % 257) / 256.0;
                    source[2] = (i * 53 % 257) / 256.0;
                    if (i % 7 == 0) {
                        source[0] -= .25;
                        source[1] += .125;
                    }
                    if (i < sizeof(edges) / sizeof(edges[0])) {
                        source[0] = edges[i];
                        source[1] = source[2] = .5;
                    }
                    ok = sixel_color_convert(input, source,
                        SIXEL_COLORSPACE_LINEAR, spaces[space]);
                    if (!ok) {
                        break;
                    }
                    for (c = 0; c < 3; ++c) {
                        pixels[i * 3 + c] = (float)input[c];
                        input[c] = pixels[i * 3 + c];
                    }
                    ok = ok && sixel_gamut_apply(fixture.mapper, input,
                        output, spaces[space]);
                    if (!ok) {
                        break;
                    }
                    for (c = 0; c < 3; ++c) {
                        expected[i * 3 + c] = (float)output[c];
                    }
                }
                ok = ok && SIXEL_SUCCEEDED(sixel_gamut_apply_float32(
                    fixture.mapper, pixels, 259, spaces[space], NULL, 0));
                ok = ok && memcmp(pixels, expected, sizeof(pixels)) == 0;
                if (!ok) {
                    fprintf(stderr, "plane/scalar mismatch: %d/%d/%d\n",
                        destination, policy, space);
                }
            }
            sixel_gamut_free(fixture.mapper);
            fixture.mapper = NULL;
        }
        sixel_cdef_free(fixture.definition);
        fixture.definition = NULL;
    }
    gamut_fixture_free(&fixture);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
