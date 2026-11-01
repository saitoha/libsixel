/* SPDX-License-Identifier: MIT */
/* Verify blue encoder workers at the consuming boundary. */
int positional_encoder_test(int, int, int);

int
test_positional_0058(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_encoder_test(2, 3, 0);
}
