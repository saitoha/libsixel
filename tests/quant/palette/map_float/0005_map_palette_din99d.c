/* SPDX-License-Identifier: MIT */
/* Observe fixed-map native precision: palette din99d. */
int test_map_float(int mode, int space, char const *path);

int
test_map_native_0005(int argc, char **argv)
{
    return argc == 2 ? test_map_float(0, 4, argv[1]) : 1;
}
