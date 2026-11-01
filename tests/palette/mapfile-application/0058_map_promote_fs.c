/* SPDX-License-Identifier: MIT */
/* Promote byte samples for native FS and verify nonzero feedback. */
int test_map_promotion(int fail, int diffuse);

int
test_map_precision_0020(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_map_promotion(0, 1);
}
