/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "gamut_test_common.h"

int
test_gamut_0006_dci_reference(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gamut_reference_check(3);
}
