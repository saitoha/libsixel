/* SPDX-License-Identifier: MIT */
/* Verify macro use environment. */
int test_delta_macro_options(int);

int
test_delta_native_0226(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_macro_options(4);
}
