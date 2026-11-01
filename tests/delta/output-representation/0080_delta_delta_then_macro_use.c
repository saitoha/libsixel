/* SPDX-License-Identifier: MIT */
/* Verify delta then macro use. */
int test_delta_macro_options(int);

int
test_delta_native_0223(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_macro_options(1);
}
