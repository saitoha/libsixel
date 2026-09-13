/* SPDX-License-Identifier: MIT */
/* Exact output for palette map clip background. */
#include "palette_map_test_common.h"

int test_pmap_0049(int argc, char **argv)
{
    static unsigned char const row[] = {
        7, 9, 11, 153, 33, 33,
    };
    static struct palette_map_case const specimen = {
        "\"1;1;2;1#2?~#3;2;80;13;13~", {0, 1}, row, 2, 1, 15U, 1, 0, 1, 0, 0};

    (void)argc;
    (void)argv;
    return test_palette_map_case(&specimen);
}
