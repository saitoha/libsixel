/* SPDX-License-Identifier: MIT */
/* Verify generated animation: packed byte fast. */
int test_packed_wire_animation(int, int);

int
test_anim_native_0011(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_packed_wire_animation(0, 4);
}
