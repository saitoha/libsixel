/* SPDX-License-Identifier: MIT */
/* Observe non gamma float work on a prepared float frame. */
#include "precision_frame_common.h"

int
test_precision_0004(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return precision_frame_samples(3, 1);
}
