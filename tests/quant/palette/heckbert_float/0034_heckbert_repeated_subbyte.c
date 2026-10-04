/* SPDX-License-Identifier: MIT */
/* Repeated sub-byte colors and signed zero retain distinct entries. */
int test_heckbert_native(int argc, char **argv);

int
test_heckbert_native_0034(int argc, char **argv)
{
    char *args[] = { "heckbert", "17", "0" };

    (void)argc;
    (void)argv;
    return test_heckbert_native(3, args);
}
