/* SPDX-License-Identifier: MIT */
/* Verify fixed palette mask: srgb linear. */
int test_fixed_mask(int space, int mode, char const *path);

int
test_fixed_mask_0002(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask(1, 0, 0);
}
