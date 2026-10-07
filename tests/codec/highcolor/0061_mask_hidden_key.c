/* SPDX-License-Identifier: MIT */
/* Hidden samples cannot become visible key representatives. */
int test_highcolor_mask(int);

int
test_highcolor_0061(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_mask(61);
}
