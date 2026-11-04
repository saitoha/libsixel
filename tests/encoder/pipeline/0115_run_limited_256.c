/* SPDX-License-Identifier: MIT */
/* A limited 256-column run preserves its split and remainder. */
int test_reserved_run(int, int, int, int, char const *);

int
test_encode_run_0115(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_reserved_run(256, 1, 0, 0, "!255@@");
}
