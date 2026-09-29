/* SPDX-License-Identifier: MIT */
/* Observe fixed-map native precision: fs cielab. */
int test_map_float(int mode, int space, char const *path);

int
test_map_native_0009(int argc, char **argv)
{
    return argc == 2 ? test_map_float(1, 3, argv[1]) : 1;
}
