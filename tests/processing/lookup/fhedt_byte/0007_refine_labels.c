/* SPDX-License-Identifier: MIT */
/* Verify refined labels against an independent integer oracle. */
int test_fh_byte_common(int mode, int resolution);

int
test_fh_byte_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fh_byte_common(6, 64);
}
