/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "edge_test_common.h"

int
test_gamut_0028_edge_invalid_boundary(int argc, char **argv)
{
    gamut_fixture_t fixture = {0};
    double const xy[8] = {.3457, .3585, .7347, .2653,
        .1596, .8404, .0366, .0001};
    double const gamma[3] = {1.8, 1.8, 1.8};
    int ok;

    /* ROMM's imaginary primaries violate this boundary model. It must
     * fail explicitly, never fall back to clipping or local MINDE. */
    fixture.definition = sixel_cdef_rgb(xy, SIXEL_COLOR_TRC_POWER, gamma);
    ok = fixture.definition != NULL &&
        SIXEL_SUCCEEDED(sixel_allocator_new(&fixture.allocator,
            NULL, NULL, NULL, NULL)) &&
        sixel_gamut_new(&fixture.mapper, fixture.definition,
            SIXEL_COLORSPACE_GAMMA, SIXEL_GAMUT_EDGE_SEEKER,
            fixture.allocator) == SIXEL_BAD_ARGUMENT &&
        fixture.mapper == NULL;
    gamut_fixture_free(&fixture);
    (void)argc;
    (void)argv;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
