/* SPDX-License-Identifier: MIT */
/* Verify fixed palette mask: full256. */
int test_fixed_mask(int space, int mode, char const *path);

int
test_fixed_mask_0011(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask(0, 2, 0);
}
