/* SPDX-License-Identifier: MIT */
#include "../../_support/processing/planner/colorspace_auto_common.h"

int
test_plan_0006_byte_precision_auto(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return colorspace_auto_check(3);
}
