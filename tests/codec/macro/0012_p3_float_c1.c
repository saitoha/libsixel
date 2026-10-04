/* SPDX-License-Identifier: MIT */
/* Verify native macro p3 float c1. */
int macro_native_contract(int scenario);

int
test_macro_0012(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return macro_native_contract(3);
}
