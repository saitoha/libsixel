/* SPDX-License-Identifier: MIT */
/* Verify or float srgb center. */
int or_native_encoder_case(int scenario);

int
test_or_native_0030(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return or_native_encoder_case(0);
}
