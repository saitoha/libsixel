/* SPDX-License-Identifier: MIT */
/* Verify fixed palette mask: ormode retry. */
int test_fixed_mask_invalid(int mode);

int
test_fixed_mask_0029(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask_invalid(1);
}
