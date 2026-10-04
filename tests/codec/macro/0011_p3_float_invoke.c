/* SPDX-License-Identifier: MIT */
/* Verify native macro p3 float invoke. */
int macro_native_contract(int scenario);

int
test_macro_0011(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return macro_native_contract(2);
}
