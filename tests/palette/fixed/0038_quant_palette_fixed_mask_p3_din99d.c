/* SPDX-License-Identifier: MIT */
/* Verify fixed palette mask: p3 din99d. */
int test_fixed_mask(int space, int mode, char const *path);

int
test_fixed_mask_0010(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask(4, 1, 0);
}
