/* SPDX-License-Identifier: MIT */
/* Observe resize preserve byte work on a prepared float frame. */
#include "precision_frame_common.h"

int
test_precision_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return precision_frame_samples(6, 1);
}
