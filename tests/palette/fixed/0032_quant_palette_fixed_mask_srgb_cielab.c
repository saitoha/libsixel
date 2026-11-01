/* SPDX-License-Identifier: MIT */
/* Verify fixed palette mask: srgb cielab. */
int test_fixed_mask(int space, int mode, char const *path);

int
test_fixed_mask_0004(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask(3, 0, 0);
}
