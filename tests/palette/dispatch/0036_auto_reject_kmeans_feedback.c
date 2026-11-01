/* SPDX-License-Identifier: MIT */
/* Observe automatic palette selection: reject kmeans feedback. */
int test_auto_palette(int mode, int space);

int
test_auto_native_0022(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_palette(17, 0);
}
