/* SPDX-License-Identifier: MIT */
/* Verify highcolor packed recovery. */
int test_highcolor_drcs(int mode);

int
test_drcs_highcolor_0021(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_drcs(7);
}
