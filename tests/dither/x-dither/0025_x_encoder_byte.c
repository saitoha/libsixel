/* SPDX-License-Identifier: MIT */
/* Verify x encoder byte at the consuming boundary. */
int positional_encoder_test(int, int, int);

int
test_positional_0049(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_encoder_test(1, 2, 0);
}
