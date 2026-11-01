/* SPDX-License-Identifier: MIT */
/* Verify float monochrome lookup: encoder light. */
int test_mono_lookup_encoder(int mode);

int
test_mono_lookup_0009(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_mono_lookup_encoder(1);
}
