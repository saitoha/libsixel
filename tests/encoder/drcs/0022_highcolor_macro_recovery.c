/* SPDX-License-Identifier: MIT */
/* Verify highcolor macro recovery. */
int test_highcolor_drcs(int mode);

int
test_drcs_highcolor_0022(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_drcs(8);
}
