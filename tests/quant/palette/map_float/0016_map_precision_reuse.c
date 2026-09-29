/* SPDX-License-Identifier: MIT */
/* Observe fixed-map native precision: precision reuse. */
int test_map_float(int mode, int space, char const *path);

int
test_map_native_0016(int argc, char **argv)
{
    return argc == 2 ? test_map_float(4, 0, argv[1]) : 1;
}
