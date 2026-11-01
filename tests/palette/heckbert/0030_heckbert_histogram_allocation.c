/* SPDX-License-Identifier: MIT */
/* A failed native allocation must not fall back to a lossy histogram. */
int test_heckbert_native(int argc, char **argv);

int
test_heckbert_native_0030(int argc, char **argv)
{
    char *args[] = { "heckbert", "14", "0" };

    (void)argc;
    (void)argv;
    /* Both the reference sort buffer and the compact tuple pool must
     * report failure without retrying the lossy legacy histogram. */
    if (test_heckbert_native(3, args) != 0) {
        return 1;
    }
    args[1] = "18";
    return test_heckbert_native(3, args);
}
