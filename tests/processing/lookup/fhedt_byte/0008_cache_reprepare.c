/* SPDX-License-Identifier: MIT */
/* Verify cached labels after reprepare against an independent integer oracle. */
int test_fh_byte_common(int mode, int resolution);

int
test_fh_byte_0008(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fh_byte_common(7, 64);
}
