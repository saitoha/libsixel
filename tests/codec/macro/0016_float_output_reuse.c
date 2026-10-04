/* SPDX-License-Identifier: MIT */
/* Verify native macro float output reuse. */
int macro_native_contract(int scenario);

int
test_macro_0016(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return macro_native_contract(7);
}
