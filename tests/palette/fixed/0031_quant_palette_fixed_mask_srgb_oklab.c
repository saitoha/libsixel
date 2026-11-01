/* SPDX-License-Identifier: MIT */
/* Verify fixed palette mask: srgb oklab. */
int test_fixed_mask(int space, int mode, char const *path);

int
test_fixed_mask_0003(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask(2, 0, 0);
}
