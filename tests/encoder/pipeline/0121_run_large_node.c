/* SPDX-License-Identifier: MIT */
/* A node wider than one output packet preserves its repeat count. */
int test_reserved_run(int, int, int, int, char const *);

int
test_encode_run_0121(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_reserved_run(20000, 0, 0, 0, "!20000@");
}
