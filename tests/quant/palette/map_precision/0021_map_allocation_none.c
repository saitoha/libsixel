/* SPDX-License-Identifier: MIT */
/* Reject native map work-buffer allocation failure with no diffusion. */
int test_map_promotion(int fail, int diffuse);

int
test_map_precision_0021(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_map_promotion(1, 0);
}
