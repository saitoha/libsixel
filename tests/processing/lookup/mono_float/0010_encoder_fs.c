/* SPDX-License-Identifier: MIT */
/* Verify float monochrome lookup: encoder fs. */
int test_mono_lookup_encoder(int mode);

int
test_mono_lookup_0010(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_mono_lookup_encoder(2);
}
