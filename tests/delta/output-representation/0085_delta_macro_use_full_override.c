/* SPDX-License-Identifier: MIT */
/* Verify macro use full override. */
int test_delta_macro_options(int);

int
test_delta_native_0228(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_macro_options(6);
}
