/* SPDX-License-Identifier: MIT */
/* Verify delta then drcs. */
int test_delta_drcs_options(int);

int
test_delta_native_0215(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_drcs_options(1);
}
