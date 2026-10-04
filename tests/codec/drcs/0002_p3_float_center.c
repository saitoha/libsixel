/* SPDX-License-Identifier: MIT */
/* Verify native DRCS p3 float center. */
int drcs_native_contract(int scenario);

int
test_drcs_native_0002(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return drcs_native_contract(1);
}
