/* SPDX-License-Identifier: MIT */
/* Exact output for palette map definitions. */
#include "palette_map_test_common.h"

int test_pmap_0043(int argc, char **argv)
{
    static unsigned char const row[] = {
        153, 33,  33, 255, 102, 33,  33,  255, 102, 33,
        33,  255, 85, 0,   0,   255, 102, 33,  33,  255,
    };
    static struct palette_map_case const specimen = {
        "#2~#3;2;80;13;13~~#4;1;120;50;100~#3~",
        {0, 1},
        row,
        5,
        6,
        9U,
        1,
        1,
        0,
        0,
        0};

    (void)argc;
    (void)argv;
    return test_palette_map_case(&specimen);
}
