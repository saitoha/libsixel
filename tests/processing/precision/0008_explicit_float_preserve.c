/* SPDX-License-Identifier: MIT */
/* Observe explicit float preserve on a prepared float frame. */
#include "precision_frame_common.h"

int
test_precision_0008(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return precision_frame_samples(7, 0);
}
