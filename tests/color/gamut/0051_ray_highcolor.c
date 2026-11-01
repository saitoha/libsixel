/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/gamut_encode_common.h"

int
test_gamut_0051_ray_highcolor(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gamut_encode_check("full-frame", "linear", 1,
        SIXEL_GAMUT_RAY_TRACE);
}
