/* SPDX-License-Identifier: MIT */
int heckbert_policy_check(int scenario);
int
test_policy_heckbert_reassignment(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return heckbert_policy_check(18);
}
