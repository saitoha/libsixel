/* SPDX-License-Identifier: MIT */
/* Observe resize float override on a prepared float frame. */
#include "../../_support/processing/precision/precision_frame_common.h"

int
test_precision_0005(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return precision_frame_samples(4, 0);
}
