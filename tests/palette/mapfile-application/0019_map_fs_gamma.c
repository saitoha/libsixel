/* SPDX-License-Identifier: MIT */
/* Observe fixed-map native precision: fs gamma. */
int test_map_float(int mode, int space, char const *path);

int
test_map_native_0006(int argc, char **argv)
{
    return argc == 2 ? test_map_float(1, 0, argv[1]) : 1;
}
