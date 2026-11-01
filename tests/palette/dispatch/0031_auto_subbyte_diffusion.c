/* SPDX-License-Identifier: MIT */
/* Observe automatic palette selection: subbyte diffusion. */
int test_auto_palette(int mode, int space);

int
test_auto_native_0017(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_palette(12, 0);
}
