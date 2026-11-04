/* SPDX-License-Identifier: MIT */
/* A limited repeat command and remainder survive a packet boundary. */
int test_reserved_run(int, int, int, int, char const *);

int
test_encode_run_0120(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_reserved_run(256, 1, 3, 0, "!255@@");
}
