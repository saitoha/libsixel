/* SPDX-License-Identifier: MIT */
/* Verify fixed palette mask: byte wide map. */
int test_fixed_mask(int space, int mode, char const *path);

int
test_fixed_mask_0027(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask(1, 18, argc > 1 ? argv[1] : "");
}
