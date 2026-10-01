/* SPDX-License-Identifier: MIT */
/* Verify fixed palette mask: srgb gamma. */
int test_fixed_mask(int space, int mode, char const *path);

int
test_fixed_mask_0001(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask(0, 0, 0);
}
