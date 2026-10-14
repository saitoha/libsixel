/* SPDX-License-Identifier: MIT */
/* Reused workers do not inherit underpainted gaps or last-column row bits. */
int test_clear_map_stream(int, int);

int
test_clear_map_0058(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_clear_map_stream(2, 3);
}
