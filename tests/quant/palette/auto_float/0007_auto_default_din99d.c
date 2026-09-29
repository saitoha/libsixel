/* SPDX-License-Identifier: MIT */
/* Observe automatic palette selection: default din99d. */
int test_auto_palette(int mode, int space);

int
test_auto_native_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_palette(0, 4);
}
