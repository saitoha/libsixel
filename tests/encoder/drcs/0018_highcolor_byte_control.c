/* SPDX-License-Identifier: MIT */
/* Verify highcolor byte control. */
int test_highcolor_drcs(int mode);

int
test_drcs_highcolor_0018(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_drcs(4);
}
