/* SPDX-License-Identifier: MIT */
/* Verify native alpha: p3 cielab. */
int test_alpha_native(int space, int mode);

int
test_alpha_native_0009(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_alpha_native(3, 1);
}
