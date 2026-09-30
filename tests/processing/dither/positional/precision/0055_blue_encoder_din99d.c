/* SPDX-License-Identifier: MIT */
/* Verify blue encoder din99d at the consuming boundary. */
int positional_encoder_test(int, int, int);

int
test_positional_0055(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_encoder_test(2, 0, 4);
}
