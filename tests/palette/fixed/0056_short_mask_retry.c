/* SPDX-License-Identifier: MIT */
/* Verify fixed palette mask: short mask retry. */
int test_fixed_mask_invalid(int mode);

int
test_fixed_mask_0028(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask_invalid(0);
}
