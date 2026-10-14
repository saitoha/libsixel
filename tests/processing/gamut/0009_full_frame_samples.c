/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include "gamut_encode_common.h"

int
test_gamut_0009_full_frame_samples(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gamut_encode_check("full-frame", "linear", 0, 0);
}
