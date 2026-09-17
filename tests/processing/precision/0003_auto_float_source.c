/* SPDX-License-Identifier: MIT */
/* Observe auto float source on a prepared float frame. */
#include "precision_frame_common.h"

int
test_precision_0003(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return precision_frame_samples(2, 0);
}
