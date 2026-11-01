/* SPDX-License-Identifier: MIT */
/* Verify frozen packet-edge bytes with unlimited DECGRI. */
int test_emit_run_stream(int, int);

int
test_emit_0063(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_emit_run_stream(0, 1);
}
