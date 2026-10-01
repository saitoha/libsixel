/* SPDX-License-Identifier: MIT */
/* Verify generated animation: mask presence. */
int test_alpha_native_animation(int, int);

int
test_anim_native_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_alpha_native_animation(0, 11);
}
