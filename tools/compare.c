/* SPDX-License-Identifier: MIT */
/* Exact file equality for build-machine checks without diffutils. */
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int
main(int argc, char **argv)
{
    /* Fixed storage avoids both file-sized allocations and small stacks. */
    static unsigned char left_buffer[65536];
    static unsigned char right_buffer[65536];
    FILE *left;
    FILE *right;
    size_t left_size;
    size_t right_size;
    int quiet;
    int status;

    quiet = 0;
    ++argv;
    --argc;
    if (argc > 0 && strcmp(argv[0], "-s") == 0) {
        quiet = 1;
        ++argv;
        --argc;
    }
    if (argc > 0 && strcmp(argv[0], "--") == 0) {
        ++argv;
        --argc;
    }
    if (argc != 2) {
        fprintf(stderr, "usage: cmp [-s] [--] FILE FILE\n");
        return 2;
    }
#ifdef _WIN32
    /* Text-mode stdin would hide CRLF and stop at an embedded Ctrl-Z. */
    if ((strcmp(argv[0], "-") == 0 || strcmp(argv[1], "-") == 0)
        && _setmode(_fileno(stdin), _O_BINARY) == -1) {
        return 2;
    }
#endif
    left = strcmp(argv[0], "-") == 0 ? stdin : fopen(argv[0], "rb");
    if (left == NULL) {
        if (!quiet) {
            perror(argv[0]);
        }
        return 2;
    }
    right = strcmp(argv[1], "-") == 0 ? stdin : fopen(argv[1], "rb");
    if (right == NULL) {
        if (!quiet) {
            perror(argv[1]);
        }
        if (left != stdin) {
            fclose(left);
        }
        return 2;
    }
    status = 0;
    /* Comparing stdin to itself must not consume alternating blocks. */
    while (left != right) {
        left_size = fread(left_buffer, 1, sizeof(left_buffer), left);
        right_size = fread(right_buffer, 1, sizeof(right_buffer), right);
        if (ferror(left) || ferror(right)) {
            status = 2;
            break;
        }
        if (left_size != right_size
            || memcmp(left_buffer, right_buffer, left_size) != 0) {
            status = 1;
            break;
        }
        if (left_size == 0) {
            break;
        }
    }
    if (left != stdin && fclose(left) != 0) {
        status = 2;
    }
    if (right != stdin && fclose(right) != 0) {
        status = 2;
    }
    if (!quiet && status != 0) {
        fprintf(stderr, "cmp: %s\n",
                status == 1 ? "files differ" : "cannot read files");
    }
    return status;
}
