/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "ray_test_common.h"

int
test_gamut_0042_ray_dci_reference(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ray_reference_check(3);
}
