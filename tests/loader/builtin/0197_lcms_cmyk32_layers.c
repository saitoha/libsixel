/* SPDX-License-Identifier: MIT */
/* Verify this public loader caller against independent ICC values. */
#include "tests/loader/cms_input_frame_common.h"

int
test_loader_0197_layers(int argc, char **argv)
{
    return cms_input_frame(argc, argv, 2);
}
