/* SPDX-License-Identifier: MIT */
/* An unlimited 4-column run preserves its exact wire spelling. */
int test_reserved_run(int, int, int, int, char const *);

int
test_encode_run_0107(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_reserved_run(4, 0, 0, 0, "!4@");
}
