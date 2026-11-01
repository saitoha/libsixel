/* SPDX-License-Identifier: MIT */
/* Verify fixed palette mask: masked opaque reuse. */
int test_fixed_mask(int space, int mode, char const *path);

int
test_fixed_mask_0016(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask(3, 7, 0);
}
