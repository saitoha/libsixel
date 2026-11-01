/* SPDX-License-Identifier: MIT */
/* Verify fixed palette mask: wide image map. */
int test_fixed_mask(int space, int mode, char const *path);

int
test_fixed_mask_0026(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fixed_mask(1, 17, argc > 1 ? argv[1] : "");
}
