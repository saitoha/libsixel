/* SPDX-License-Identifier: MIT */
/* Observe byte request linear source on a prepared float frame. */
#include "../../_support/processing/precision/precision_frame_common.h"

int
test_precision_0002(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return precision_frame_samples(0, 1);
}
