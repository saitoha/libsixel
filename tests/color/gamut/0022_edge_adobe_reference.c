/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/edge_test_common.h"

int
test_gamut_0022_edge_adobe_reference(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return edge_reference_check(2);
}
