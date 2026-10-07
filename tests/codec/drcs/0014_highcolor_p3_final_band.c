/* SPDX-License-Identifier: MIT */
/* Verify highcolor p3 final band. */
int test_highcolor_drcs(int mode);

int
test_drcs_highcolor_0014(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_drcs(0);
}
