/* SPDX-License-Identifier: MIT */
/* Observe matrix adoption without asserting close-primary solve accuracy. */
#include <stddef.h>

int test_loader_0198_png_chrm_samples(int argc, char **argv);

int
test_loader_0202_png_chrm_accept(int argc, char **argv)
{
    char *args[5];

    if (argc != 3) {
        return 1;
    }
    args[0] = argv[0];
    args[1] = argv[1];
    args[2] = argv[2];
    args[3] = "matrix";
    args[4] = NULL;
    return test_loader_0198_png_chrm_samples(4, args);
}
