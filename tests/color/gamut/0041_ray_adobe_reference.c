/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "../../_support/processing/gamut/ray_test_common.h"

int
test_gamut_0041_ray_adobe_reference(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ray_reference_check(2);
}
