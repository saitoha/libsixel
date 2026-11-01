/* SPDX-License-Identifier: MIT */
/* A failed native allocation must not fall back to a lossy histogram. */
int test_heckbert_native(int argc, char **argv);

int
test_heckbert_native_0031(int argc, char **argv)
{
    char *args[] = { "heckbert", "15", "0" };

    (void)argc;
    (void)argv;
    return test_heckbert_native(3, args);
}
