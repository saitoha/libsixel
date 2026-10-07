/* SPDX-License-Identifier: MIT */
/* Fractional RGB blends retain the colored background. */
int test_highcolor_packed(int);

int
test_highcolor_0076(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_packed(76);
}
