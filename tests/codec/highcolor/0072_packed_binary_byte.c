/* SPDX-License-Identifier: MIT */
/* Binary alpha keeps RGB888 despite bypassed clustering. */
int test_highcolor_packed(int);

int
test_highcolor_0072(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_packed(72);
}
