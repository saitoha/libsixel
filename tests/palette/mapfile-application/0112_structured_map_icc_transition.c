/* SPDX-License-Identifier: MIT */
int test_map_cluster_icc_transition(char const *path);

int
test_map_cluster_0112(int argc, char **argv)
{
    return argc == 2 ? test_map_cluster_icc_transition(argv[1]) : 1;
}
