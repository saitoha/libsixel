/* SPDX-License-Identifier: MIT */
/* Exact output for palette map redefinition. */
#include "palette_map_test_common.h"

int test_pmap_0044(int argc, char **argv)
{
    static unsigned char const row[] = {
        153, 33, 33, 255, 102, 33, 33, 255,
    };
    static struct palette_map_case const specimen = {
        "#2~#2;2;80;13;13~", {0, 1}, row, 2, 6, 9U, 1, 1, 0, 0, 0};

    (void)argc;
    (void)argv;
    return test_palette_map_case(&specimen);
}
