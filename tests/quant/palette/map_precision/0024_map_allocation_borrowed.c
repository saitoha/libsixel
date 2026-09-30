/* SPDX-License-Identifier: MIT */
/* Keep caller-owned indices intact when native float allocation fails. */
int test_map_promotion(int fail, int diffuse);

int
test_map_precision_0024(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_map_promotion(2, 0);
}
