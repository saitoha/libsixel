/* SPDX-License-Identifier: MIT */
/* Verify animation handoff storage ownership. */
int test_animation_copy(int);

int
test_anim_native_0017(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_animation_copy(0);
}
