/* SPDX-License-Identifier: MIT */
/* Observe fixed-map native precision: fs din99d. */
int test_map_float(int mode, int space, char const *path);

int
test_map_native_0010(int argc, char **argv)
{
    return argc == 2 ? test_map_float(1, 4, argv[1]) : 1;
}
