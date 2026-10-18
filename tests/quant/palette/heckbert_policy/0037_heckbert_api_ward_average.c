/* SPDX-License-Identifier: MIT */
int heckbert_defaults_check(int diffusion, int representative, int ward,
                            int expected);
int
test_policy_heckbert_api_ward_average(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return heckbert_defaults_check(3, 0, 1, 2);
}
