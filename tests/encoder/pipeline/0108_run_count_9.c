/* SPDX-License-Identifier: MIT */
/* An unlimited 9-column run preserves its exact wire spelling. */
int test_reserved_run(int, int, int, int, char const *);

int
test_encode_run_0108(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_reserved_run(9, 0, 0, 0, "!9@");
}
