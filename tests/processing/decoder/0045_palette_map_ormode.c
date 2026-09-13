/* SPDX-License-Identifier: MIT */
/* Exact output for palette map ormode. */
#include "palette_map_test_common.h"

int test_pmap_0045(int argc, char **argv)
{
    static unsigned char const row[] = {
        170, 0, 255, 255, 85, 0, 0, 255,
    };
    static struct palette_map_case const specimen = {
        "\"1;1;2;6#1;2;100;0;0#2;2;0;100;0#3;2;0;0;100#1~~$#2~",
        {7, 5},
        row,
        2,
        6,
        1U,
        1,
        0,
        0,
        0,
        0};

    (void)argc;
    (void)argv;
    return test_palette_map_case(&specimen);
}
