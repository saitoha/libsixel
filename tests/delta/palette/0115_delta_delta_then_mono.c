/* SPDX-License-Identifier: MIT */
/* Verify delta then mono. */
int test_delta_mono_options(int);

int
test_delta_native_0202(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_mono_options(1);
}
