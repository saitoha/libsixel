/* SPDX-License-Identifier: MIT */
/* Observe fixed-map native precision: byte. */
int test_map_float(int mode, int space, char const *path);

int
test_map_native_0014(int argc, char **argv)
{
    return argc == 2 ? test_map_float(2, 0, argv[1]) : 1;
}
