/* SPDX-License-Identifier: MIT */
/* Repeated colors retain exact populations within a compact budget. */
int test_heckbert_native(int argc, char **argv);

int
test_heckbert_native_0033(int argc, char **argv)
{
    char *args[] = { "heckbert", "16", "0" };

    (void)argc;
    (void)argv;
    return test_heckbert_native(3, args);
}
