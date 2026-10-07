/* SPDX-License-Identifier: MIT */
/* Verify highcolor p3 hls. */
int test_highcolor_drcs(int mode);

int
test_drcs_highcolor_0017(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_drcs(3);
}
