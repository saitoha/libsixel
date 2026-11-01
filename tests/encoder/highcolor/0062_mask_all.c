/* SPDX-License-Identifier: MIT */
/* An all-masked frame declares its extent without painting. */
int test_highcolor_mask(int);

int
test_highcolor_0062(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_mask(62);
}
