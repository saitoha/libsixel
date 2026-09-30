/* SPDX-License-Identifier: MIT */
/* Verify thread-requested discrete grid against an independent integer oracle. */
int test_fh_byte_common(int mode, int resolution);

int
test_fh_byte_0006(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fh_byte_common(5, 64);
}
