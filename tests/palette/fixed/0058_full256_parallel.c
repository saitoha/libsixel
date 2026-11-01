/* SPDX-License-Identifier: MIT */
/* Verify fixed palette mask: full256 parallel. */
int test_fixed_mask(int space, int mode, char const *path);

int
test_fixed_mask_0030(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask(0, 21, 0);
}
