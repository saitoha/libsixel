/* SPDX-License-Identifier: MIT */
/* Exact output for palette map gpu. */
#include "palette_map_test_common.h"

int test_pmap_0051(int argc, char **argv)
{
    static unsigned char const row[] = {
        102, 33, 33, 255, 102, 33, 33, 255, 102, 33, 33, 255, 102, 33, 33, 255,
        102, 33, 33, 255, 102, 33, 33, 255, 102, 33, 33, 255, 102, 33, 33, 255,
    };
    static struct palette_map_case const specimen = {
        "\"1;1;8;12#2;2;80;13;13#2!8~-#2!8~",
        {0, 1},
        row,
        8,
        12,
        1U,
        1,
        1,
        0,
        0,
        1};

    (void)argc;
    (void)argv;
    return test_palette_map_case(&specimen);
}
