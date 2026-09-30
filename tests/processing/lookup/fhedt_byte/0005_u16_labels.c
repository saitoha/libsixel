/* SPDX-License-Identifier: MIT */
/* Verify 16-bit labels and duplicate seeds against an independent integer oracle. */
int test_fh_byte_common(int mode, int resolution);

int
test_fh_byte_0005(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_fh_byte_common(4, 64);
}
