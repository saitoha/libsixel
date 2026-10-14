/* SPDX-License-Identifier: MIT */
/* Verify classify paint rows. */
int test_classify_stream(int, char const *);

int
test_classify_0051(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_classify_stream(1, "#1KB???o$#0B???oK$#2oKB");
}
