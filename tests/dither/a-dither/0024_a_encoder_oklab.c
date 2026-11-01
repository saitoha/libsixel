/* SPDX-License-Identifier: MIT */
/* Verify a encoder oklab at the consuming boundary. */
int positional_encoder_test(int, int, int);

int
test_positional_0037(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return positional_encoder_test(0, 0, 2);
}
