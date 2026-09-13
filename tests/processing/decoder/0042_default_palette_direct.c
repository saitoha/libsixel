/* SPDX-License-Identifier: MIT */
/* Exact output for default palette direct. */
#include "palette_map_test_common.h"

int test_pmap_0042(int argc, char **argv)
{
    static unsigned char const row[] = {
        204, 33, 33, 255, 255, 255, 255, 255,
    };
    static struct palette_map_case const specimen = {
        "#2~#256~", {0, 1}, row, 2, 6, 1U, 0, 0, 0, 0, 0};

    (void)argc;
    (void)argv;
    return test_palette_map_case(&specimen);
}
