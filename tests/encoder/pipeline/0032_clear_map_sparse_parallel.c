/* SPDX-License-Identifier: MIT */
/* Sparse parallel bands do not inherit underpainted gaps. */
int test_clear_map_stream(int, int);

int
test_clear_map_0060(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_clear_map_stream(2, 5);
}
