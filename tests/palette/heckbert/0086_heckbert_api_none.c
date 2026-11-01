/* SPDX-License-Identifier: MIT */
int heckbert_defaults_check(int diffusion, int representative, int ward,
                            int expected);
int
test_policy_heckbert_api_none(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return heckbert_defaults_check(1, 0, 0, 3);
}
