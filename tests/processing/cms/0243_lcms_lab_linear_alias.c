/* SPDX-License-Identifier: MIT */
/* Verify one LCMS float input-unit and buffer-layout contract. */
#include "lcms_input_units_common.h"

int
test_cms_0243(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return lcms_input_units(1, 0, 1);
}
