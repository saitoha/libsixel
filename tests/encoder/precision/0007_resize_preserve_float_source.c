/* SPDX-License-Identifier: MIT */
/* Observe resize preserve float source on a prepared float frame. */
#include "../../_support/processing/precision/precision_frame_common.h"

int
test_precision_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return precision_frame_samples(6, 1);
}
