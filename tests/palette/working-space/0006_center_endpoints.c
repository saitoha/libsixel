/* SPDX-License-Identifier: MIT */
int test_cluster_precision(int mode);

int
test_cluster_precision_0006(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_cluster_precision(5);
}
