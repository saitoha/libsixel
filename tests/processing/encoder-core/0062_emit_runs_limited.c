/* SPDX-License-Identifier: MIT */
/* Verify frozen repeat-boundary bytes with limited DECGRI. */
int test_emit_run_stream(int, int);

int
test_emit_0062(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_emit_run_stream(1, 0);
}
