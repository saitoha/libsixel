/* SPDX-License-Identifier: MIT */
/* Verify mono full override. */
int test_delta_mono_options(int);

int
test_delta_native_0204(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_mono_options(3);
}
