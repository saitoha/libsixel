/* SPDX-License-Identifier: MIT */
/* Observe linear byte boundary on a prepared float frame. */
#include "precision_frame_common.h"

int
test_precision_0002(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return precision_frame_samples(0, 1);
}
