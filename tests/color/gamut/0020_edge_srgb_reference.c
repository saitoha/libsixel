/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/edge_test_common.h"

int
test_gamut_0020_edge_srgb_reference(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return edge_reference_check(0);
}
