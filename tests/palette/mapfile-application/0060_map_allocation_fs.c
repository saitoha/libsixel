/* SPDX-License-Identifier: MIT */
/* Reject native map work-buffer allocation failure with hard FS. */
int test_map_promotion(int fail, int diffuse);

int
test_map_precision_0022(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_map_promotion(1, 1);
}
