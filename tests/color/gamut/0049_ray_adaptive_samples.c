/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/gamut_encode_common.h"

int
test_gamut_0049_ray_adaptive_samples(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gamut_encode_check("adaptive-grid", "linear", 0,
        SIXEL_GAMUT_RAY_TRACE);
}
