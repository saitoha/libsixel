/* SPDX-License-Identifier: MIT */
/* Encoder hard AUTO preserves 32 native colors and RGB101 output. */
int test_auto_cover_encoder(int);

int
test_auto_cover_0009(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_auto_cover_encoder(1);
}
