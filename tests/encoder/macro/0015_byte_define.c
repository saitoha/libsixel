/* SPDX-License-Identifier: MIT */
/* Verify native macro byte define. */
int macro_native_contract(int scenario);

int
test_macro_0015(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return macro_native_contract(6);
}
