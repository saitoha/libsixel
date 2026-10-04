/* SPDX-License-Identifier: MIT */
/* Verify native macro float animation replay. */
int macro_native_contract(int scenario);

int
test_macro_0014(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return macro_native_contract(5);
}
