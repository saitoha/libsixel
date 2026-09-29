/* SPDX-License-Identifier: MIT */
/* Observe fixed-map native precision: export cielab. */
int test_map_float(int mode, int space, char const *path);

int
test_map_native_0024(int argc, char **argv)
{
    return argc == 2 ? test_map_float(7, 3, argv[1]) : 1;
}
