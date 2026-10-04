/* SPDX-License-Identifier: MIT */
/* Verify native DRCS p3 builtin. */
int drcs_native_contract(int scenario);

int
test_drcs_native_0005(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return drcs_native_contract(4);
}
