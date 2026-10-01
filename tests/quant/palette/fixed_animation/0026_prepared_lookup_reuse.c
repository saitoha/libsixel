/* SPDX-License-Identifier: MIT */
/* Stable fixed frames retain their prepared lookup owner. */
int test_builtin_animation(int, int, int, int);

int
test_fixed_animation_0026(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_builtin_animation(0, 0, 0, 14);
}
