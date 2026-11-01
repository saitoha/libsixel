/* SPDX-License-Identifier: MIT */
/* A high-color source mask explicitly requests P2=0 for clear. */
int test_highcolor_mask(int);

int
test_highcolor_0054(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_mask(54);
}
