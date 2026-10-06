/* SPDX-License-Identifier: MIT */
/* Repeated keys retain paint-time colors across pending passes. */
int test_highcolor_band(int, int, int);

int
test_highcolor_0042(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_band(42, 64, 19);
}
