/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/gamut_encode_common.h"

int
test_gamut_0032_edge_oklab_workspace(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gamut_encode_check("full-frame", "oklab", 0,
        SIXEL_GAMUT_EDGE_SEEKER);
}
