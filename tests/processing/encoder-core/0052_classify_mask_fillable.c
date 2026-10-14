/* SPDX-License-Identifier: MIT */
/* Verify classify mask fillable. */
int test_classify_stream(int, char const *);

int
test_classify_0052(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_classify_stream(2, "#1A@A@$#0?A@A$");
}
