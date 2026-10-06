/* SPDX-License-Identifier: MIT */
/* Verify macro float recovery. */
int test_delta_macro_recovery(int);

int
test_delta_native_0231(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_delta_macro_recovery(1);
}
