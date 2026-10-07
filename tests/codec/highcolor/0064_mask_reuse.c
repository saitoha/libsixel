/* SPDX-License-Identifier: MIT */
/* P3 encoder reuse clears and replaces borrowed masks. */
int test_highcolor_mask(int);

int
test_highcolor_0064(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_mask(64);
}
