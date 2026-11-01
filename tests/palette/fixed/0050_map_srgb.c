/* SPDX-License-Identifier: MIT */
/* Verify fixed palette mask: map srgb. */
int test_fixed_mask(int space, int mode, char const *path);

int
test_fixed_mask_0022(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask(0, 13, argc > 1 ? argv[1] : "");
}
