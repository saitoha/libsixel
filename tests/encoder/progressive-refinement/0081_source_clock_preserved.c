/* SPDX-License-Identifier: MIT */
int test_progressive_schedule_case(int mode);

int
test_progressive_0111(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_progressive_schedule_case(111);
}
