/* SPDX-License-Identifier: MIT */
/* Observe fixed-map native precision: image gamma. */
int test_map_float(int mode, int space, char const *path);

int
test_map_native_0018(int argc, char **argv)
{
    return argc == 2 ? test_map_float(6, 0, argv[1]) : 1;
}
