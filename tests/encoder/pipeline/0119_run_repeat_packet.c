/* SPDX-License-Identifier: MIT */
/* A repeat command survives a packet boundary. */
int test_reserved_run(int, int, int, int, char const *);

int
test_encode_run_0119(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_reserved_run(4, 0, 1, 0, "!4@");
}
