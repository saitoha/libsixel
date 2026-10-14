/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "edge_test_common.h"

int
test_gamut_0023_edge_dci_reference(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return edge_reference_check(3);
}
