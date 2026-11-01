/* SPDX-License-Identifier: MIT */
/* Verify this public loader caller against independent ICC values. */
#include "tests/_support/loader/cms_input_frame_common.h"

int
test_loader_0196_missing(int argc, char **argv)
{
    return cms_input_frame(argc, argv, 1);
}
