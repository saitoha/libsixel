/* SPDX-License-Identifier: MIT */
/* Verify native DRCS p3 c1 body. */
int drcs_native_contract(int scenario);

int
test_drcs_native_0003(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return drcs_native_contract(2);
}
