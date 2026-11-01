/* SPDX-License-Identifier: MIT */
int test_residual_gate(int cells, int blue);

int
test_residual_0028(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_residual_gate(0, 3);
}
