/* SPDX-License-Identifier: MIT */
/* Observe automatic palette selection: byte hard kmeans. */
int test_auto_palette(int mode, int space);

int
test_auto_native_0019(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_palette(14, 0);
}
