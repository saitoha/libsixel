/* SPDX-License-Identifier: MIT */
/* A limited 258-column run preserves its split and remainder. */
int test_reserved_run(int, int, int, int, char const *);

int
test_encode_run_0116(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_reserved_run(258, 1, 0, 0, "!255@@@@");
}
