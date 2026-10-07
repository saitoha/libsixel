/* SPDX-License-Identifier: MIT */
/* Verify highcolor output reuse. */
int test_highcolor_drcs(int mode);

int
test_drcs_highcolor_0019(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_drcs(5);
}
