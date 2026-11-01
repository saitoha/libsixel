/* SPDX-License-Identifier: MIT */
/* Verify highcolor writer restore. */
int test_highcolor_drcs(int mode);

int
test_drcs_highcolor_0024(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_highcolor_drcs(10);
}
