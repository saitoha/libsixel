/* SPDX-License-Identifier: MIT */
/* Observe automatic palette selection: irrelevant heckbert feedback. */
int test_auto_palette(int mode, int space);

int
test_auto_native_0023(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_palette(18, 0);
}
