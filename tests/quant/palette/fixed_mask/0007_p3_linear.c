/* SPDX-License-Identifier: MIT */
/* Verify fixed palette mask: p3 linear. */
int test_fixed_mask(int space, int mode, char const *path);

int
test_fixed_mask_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask(1, 1, 0);
}
