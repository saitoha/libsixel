/* SPDX-License-Identifier: MIT */
/* Exact file equality for build-machine checks without diffutils. */
/* Expose descriptor inspection even with strict ISO C compiler flags. */
#if !defined(_WIN32) && !defined(__VMS) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static int
compare_directory(FILE *stream)
{
#ifdef _WIN32
    struct _stat info;

    return _fstat(_fileno(stream), &info) != 0
        || (info.st_mode & _S_IFMT) == _S_IFDIR;
#else
    struct stat info;

    return fstat(fileno(stream), &info) != 0 || S_ISDIR(info.st_mode);
#endif
}

static int
compare_main(int argc, char **argv)
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
#ifdef __VMS
    char input_name[4096];
#endif

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
#ifdef __VMS
    /* The inherited text stream synthesizes a final RMS record newline.
     * Reopen its actual file or mailbox before reading any input bytes.
     * fdopen does not change that mode and freopen(NULL, ...) is unsupported.
     */
    if (strcmp(argv[0], "-") == 0 || strcmp(argv[1], "-") == 0) {
        if (fgetname(stdin, input_name, 1) == NULL
            || freopen(input_name, "rb", stdin) == NULL) {
            return 2;
        }
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
    /* Hurd can read directory bytes successfully; reject them explicitly. */
    status = compare_directory(left) || compare_directory(right) ? 2 : 0;
    /* Comparing stdin to itself must not consume alternating blocks. */
    while (status == 0 && left != right) {
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

int
main(int argc, char **argv)
{
    int status;
#ifdef __VMS
    extern void __posix_exit(int);
#endif

    status = compare_main(argc, argv);
#ifdef __VMS
    /* Native status 1 means success; GNV needs the POSIX encoding instead. */
    __posix_exit(status);
#endif
    return status;
}
