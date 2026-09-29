/* SPDX-License-Identifier: MIT */
/* Observe fixed-map native precision: act. */
int test_map_float(int mode, int space, char const *path);

int
test_map_native_0011(int argc, char **argv)
{
    return argc == 2 ? test_map_float(0, 0, argv[1]) : 1;
}
