/* SPDX-License-Identifier: MIT */
/* Verify one fixed-target Lab input-unit and buffer contract. */
#include "tests/processing/cms/colorsync_lab_units_common.h"

int
test_cms_0247(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return colorsync_lab_units(0, 1);
}
