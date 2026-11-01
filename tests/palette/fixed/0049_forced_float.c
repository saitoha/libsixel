/* SPDX-License-Identifier: MIT */
/* Verify fixed palette mask: forced float. */
int test_fixed_mask(int space, int mode, char const *path);

int
test_fixed_mask_0021(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask(0, 12, 0);
}
