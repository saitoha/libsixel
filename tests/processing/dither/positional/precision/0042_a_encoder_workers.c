/* SPDX-License-Identifier: MIT */
/* Verify a encoder workers at the consuming boundary. */
int positional_encoder_test(int, int, int);

int
test_positional_0042(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_encoder_test(0, 3, 0);
}
