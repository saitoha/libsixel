/* SPDX-License-Identifier: MIT */
/* Verify highcolor p3 v3 grid. */
int test_highcolor_drcs(int mode);

int
test_drcs_highcolor_0016(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_drcs(2);
}
