/* SPDX-License-Identifier: MIT */
/* Verify a encoder gamma at the consuming boundary. */
int positional_encoder_test(int, int, int);

int
test_positional_0035(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_encoder_test(0, 0, 0);
}
