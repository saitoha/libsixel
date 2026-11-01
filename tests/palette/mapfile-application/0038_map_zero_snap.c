/* SPDX-License-Identifier: MIT */
/* Observe fixed-map native precision: zero snap. */
int test_map_float(int mode, int space, char const *path);

int
test_map_native_0025(int argc, char **argv)
{
    return argc == 2 ? test_map_float(8, 0, argv[1]) : 1;
}
