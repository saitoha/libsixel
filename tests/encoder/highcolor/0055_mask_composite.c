/* SPDX-License-Identifier: MIT */
/* A retained composite-fallback mask requests P2=1. */
int test_highcolor_mask(int);

int
test_highcolor_0055(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_mask(55);
}
