/* SPDX-License-Identifier: MIT */
/* Verify fixed palette mask: p3 cielab. */
int test_fixed_mask(int space, int mode, char const *path);

int
test_fixed_mask_0009(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask(3, 1, 0);
}
