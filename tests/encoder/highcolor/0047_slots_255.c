/* SPDX-License-Identifier: MIT */
/* Exactly 255 keys finish without a pending pass. */
int test_highcolor_band(int, int, int);

int
test_highcolor_0047(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_band(47, 255, 1);
}
