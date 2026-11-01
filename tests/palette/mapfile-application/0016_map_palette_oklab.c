/* SPDX-License-Identifier: MIT */
/* Observe fixed-map native precision: palette oklab. */
int test_map_float(int mode, int space, char const *path);

int
test_map_native_0003(int argc, char **argv)
{
    return argc == 2 ? test_map_float(0, 2, argv[1]) : 1;
}
