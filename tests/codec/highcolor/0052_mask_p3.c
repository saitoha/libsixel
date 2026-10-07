/* SPDX-License-Identifier: MIT */
/* P3 RGB preserves visible colors and separated omissions. */
int test_highcolor_mask(int);

int
test_highcolor_0052(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_mask(52);
}
