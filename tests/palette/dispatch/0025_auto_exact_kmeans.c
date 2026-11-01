/* SPDX-License-Identifier: MIT */
/* Observe automatic palette selection: exact kmeans. */
int test_auto_palette(int mode, int space);

int
test_auto_native_0011(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_palette(6, 0);
}
