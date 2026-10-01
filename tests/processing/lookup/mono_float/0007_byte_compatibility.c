/* SPDX-License-Identifier: MIT */
/* Verify float monochrome lookup: byte compatibility. */
int test_mono_lookup_values(int mode);

int
test_mono_lookup_0007(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_mono_lookup_values(6);
}
