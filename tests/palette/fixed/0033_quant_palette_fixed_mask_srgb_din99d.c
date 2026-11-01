/* SPDX-License-Identifier: MIT */
/* Verify fixed palette mask: srgb din99d. */
int test_fixed_mask(int space, int mode, char const *path);

int
test_fixed_mask_0005(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask(4, 0, 0);
}
