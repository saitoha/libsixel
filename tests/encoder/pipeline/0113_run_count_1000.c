/* SPDX-License-Identifier: MIT */
/* An unlimited 1000-column run preserves its exact wire spelling. */
int test_reserved_run(int, int, int, int, char const *);

int
test_encode_run_0113(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_reserved_run(1000, 0, 0, 0, "!1000@");
}
