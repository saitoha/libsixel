/* SPDX-License-Identifier: MIT */
/* Verify blue encoder oklab at the consuming boundary. */
int positional_encoder_test(int, int, int);

int
test_positional_0053(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_encoder_test(2, 0, 2);
}
