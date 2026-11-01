/* SPDX-License-Identifier: MIT */
/* Verify fixed palette mask: inverted mono. */
int test_fixed_mask(int space, int mode, char const *path);

int
test_fixed_mask_0025(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask(4, 16, 0);
}
