/* SPDX-License-Identifier: MIT */
int test_repair_palette_case(int mode);

int
test_progressive_0003(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_repair_palette_case(3);
}
