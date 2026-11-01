/* SPDX-License-Identifier: MIT */
/* Observe fixed-map native precision: promote. */
int test_map_float(int mode, int space, char const *path);

int
test_map_native_0015(int argc, char **argv)
{
    return argc == 2 ? test_map_float(3, 0, argv[1]) : 1;
}
