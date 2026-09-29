/* SPDX-License-Identifier: MIT */
/* Observe automatic palette selection: soft kmeans. */
int test_auto_palette(int mode, int space);

int
test_auto_native_0012(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_palette(7, 0);
}
