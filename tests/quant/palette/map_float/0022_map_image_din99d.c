/* SPDX-License-Identifier: MIT */
/* Observe fixed-map native precision: image din99d. */
int test_map_float(int mode, int space, char const *path);

int
test_map_native_0022(int argc, char **argv)
{
    return argc == 2 ? test_map_float(6, 4, argv[1]) : 1;
}
