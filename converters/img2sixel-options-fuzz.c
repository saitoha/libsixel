/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if HAVE_GETOPT_H
# include <getopt.h>
#endif

#include "getopt_stub.h"

extern int img2sixel_main(int argc, char *argv[]);

#if defined(__AFL_HAVE_MANUAL_CONTROL)
__AFL_EXTERN_C void __AFL_INIT(void);
#endif

static unsigned char
fuzz_next_byte(unsigned char const *data, size_t size, size_t *offset)
{
    unsigned char value;

    if (*offset >= size) {
        return 0;
    }

    value = data[*offset];
    (*offset)++;
    return value;
}

static char *
copy_cstring(char const *value)
{
    char *copy;

    copy = NULL;
    if (value == NULL) {
        return NULL;
    }

    copy = (char *)malloc(strlen(value) + 1);
    if (copy == NULL) {
        return NULL;
    }

    (void)strcpy(copy, value);
    return copy;
}

static int
push_argument(char **argv, char **owned, int *argc, int *owned_count,
              char const *value)
{
    char *copy;

    if (*argc >= 64 || *owned_count >= 64) {
        return -1;
    }

    copy = copy_cstring(value);
    if (copy == NULL) {
        return -1;
    }

    argv[*argc] = copy;
    owned[*owned_count] = copy;
    (*argc)++;
    (*owned_count)++;

    return 0;
}

static int
push_option(char **argv, char **owned, int *argc, int *owned_count,
            char opt, char const *arg)
{
    char optbuf[3];

    optbuf[0] = '-';
    optbuf[1] = opt;
    optbuf[2] = '\0';

    if (push_argument(argv, owned, argc, owned_count, optbuf) != 0) {
        return -1;
    }

    if (arg != NULL) {
        if (push_argument(argv, owned, argc, owned_count, arg) != 0) {
            return -1;
        }
    }

    return 0;
}

static void
release_arguments(char **owned, int owned_count)
{
    int i;

    for (i = 0; i < owned_count; i++) {
        free(owned[i]);
    }
}

static int
write_fuzz_file(char const *template_path,
                unsigned char const *data,
                size_t size,
                char *path_out,
                size_t path_size)
{
    int fd;
    int close_result;
    ssize_t written;
    unsigned char zero;

    if (path_size == 0) {
        return -1;
    }

    (void)strncpy(path_out, template_path, path_size - 1);
    path_out[path_size - 1] = '\0';

    fd = mkstemp(path_out);
    if (fd < 0) {
        return -1;
    }

    zero = 0;
    if (size == 0) {
        written = write(fd, &zero, 1);
    } else {
        written = write(fd, data, size);
    }

    close_result = close(fd);
    if (written < 0 || close_result != 0) {
        (void)unlink(path_out);
        return -1;
    }

    return 0;
}

static char const *
select_from_list(char const *const *values, size_t count,
                 unsigned char seed)
{
    if (count == 0) {
        return NULL;
    }

    return values[seed % count];
}

static void
apply_environment(unsigned char const *data, size_t size, size_t *offset)
{
    unsigned char mask;

    mask = fuzz_next_byte(data, size, offset);
    if (mask & 0x01) {
        (void)setenv("SIXEL_FLOAT32_DITHER", "1", 1);
    }
    if (mask & 0x02) {
        (void)setenv("SIXEL_THREADS", "1", 1);
    }
    if (mask & 0x04) {
        (void)setenv("SIXEL_DITHER_LOOKUP_POLICY", "none", 1);
    }
    if (mask & 0x08) {
        (void)setenv("SIXEL_DITHER_LOOKUP_POLICY", "vpte", 1);
    }
}

/*
 * AFL input layout (single buffer):
 *
 *   +---------+-----------+-------------------------+
 *   | Byte 0  | Byte 1    | Bytes 2..end            |
 *   +---------+-----------+-------------------------+
 *   | opt cnt | env mask  | option seeds + image    |
 *   +---------+-----------+-------------------------+
 *
 * The remaining bytes drive:
 *   1) option selection (short opt + choice index)
 *   2) optional extra flags (quantize + palette size)
 *   3) raw image bytes written to a temporary input file
 *
 * This keeps the harness deterministic while still allowing AFL to
 * mutate the option mix and the binary payload independently.
 *
 * Flow overview:
 *
 *   +--------------+     +---------------------+
 *   | AFL input    | --> | argv/env setup      |
 *   +--------------+     +---------------------+
 *                                 |
 *                                 v
 *                         +---------------------+
 *                         | img2sixel_main()    |
 *                         +---------------------+
 *
 * Cleanup is handled by:
 *   - unlinking temporary input/output files
 *   - freeing argv entries stored in owned[]
 */
int
main(void)
{
    unsigned char data[4096];
    ssize_t read_size;
    size_t size;
    size_t offset;
    unsigned char option_count;
    char input_path[64];
    char output_path[64];
    char *argv[64];
    char *owned[64];
    int argc;
    int owned_count;
    int run;
    int i;
    unsigned char selector;
    unsigned char seed;
    char numbuf[16];
    char opt;
    char const *choice;
    char const *model;
    unsigned int colors;
    int result;

    static char const *const colorspaces[] = {
        "gamma",
        "linear",
        "oklab",
        "cielab",
        "din99d"
    };
    static char const *const resample_filters[] = {
        "nearest",
        "gaussian",
        "hanning",
        "hamming",
        "bilinear",
        "welsh",
        "bicubic",
        "lanczos2",
        "lanczos3",
        "lanczos4"
    };
    static char const *const diffusion_modes[] = {
        "auto",
        "none",
        "fs",
        "atkinson",
        "jajuni",
        "stucki",
        "burkes",
        "sierra1",
        "sierra2",
        "sierra3",
        "a_dither",
        "x_dither",
        "lso2"
    };
    static char const *const diffusion_scans[] = {
        "auto",
        "raster",
        "serpentine"
    };
    static char const *const diffusion_carries[] = {
        "auto",
        "direct",
        "carry"
    };
    static char const *const quality_modes[] = {
        "auto",
        "low",
        "high",
        "full"
    };
    static char const *const width_height_values[] = {
        "auto",
        "1",
        "2",
        "10",
        "64",
        "80%",
        "120%",
        "10c",
        "20c",
        "10px",
        "32px"
    };
    static char const *const palette_types[] = {
        "auto",
        "hls",
        "rgb"
    };
    static char const *const quantize_models[] = {
        "auto",
        "heckbert",
        "kmeans"
    };
    static char const *const option_flags[] = {
        "7",
        "8",
        "e",
        "i",
        "I",
        "g",
        "S",
        "u",
        "v",
        "O"
    };

#if defined(__AFL_HAVE_MANUAL_CONTROL)
    __AFL_INIT();
#endif

    for (run = 0; run < 1000; run++) {
        read_size = read(STDIN_FILENO, data, sizeof(data));
        if (read_size <= 0) {
            break;
        }

        size = (size_t)read_size;
        offset = 0;
        argc = 0;
        owned_count = 0;

        if (push_argument(argv, owned, &argc, &owned_count,
                          "img2sixel-options-fuzz") != 0) {
            return 0;
        }

        (void)strncpy(input_path, "/tmp/img2sixel-fuzz-input-XXXXXX",
                      sizeof(input_path) - 1);
        input_path[sizeof(input_path) - 1] = '\0';
        (void)strncpy(output_path, "/tmp/img2sixel-fuzz-out-XXXXXX",
                      sizeof(output_path) - 1);
        output_path[sizeof(output_path) - 1] = '\0';

        option_count = fuzz_next_byte(data, size, &offset) & 0x0f;
        apply_environment(data, size, &offset);

        if (write_fuzz_file(input_path, data + offset,
                            size - offset, input_path,
                            sizeof(input_path)) != 0) {
            release_arguments(owned, owned_count);
            return 0;
        }

        if (write_fuzz_file(output_path, NULL, 0, output_path,
                            sizeof(output_path)) != 0) {
            (void)unlink(input_path);
            release_arguments(owned, owned_count);
            return 0;
        }

        if (push_option(argv, owned, &argc, &owned_count, 'o',
                        output_path) != 0) {
            (void)unlink(input_path);
            (void)unlink(output_path);
            release_arguments(owned, owned_count);
            return 0;
        }

        for (i = 0; i < option_count; i++) {
            selector = fuzz_next_byte(data, size, &offset);
            seed = fuzz_next_byte(data, size, &offset);

            if (selector < 20) {
                opt = option_flags[selector %
                                   (sizeof(option_flags) /
                                    sizeof(option_flags[0]))][0];
                if (push_option(argv, owned, &argc, &owned_count, opt,
                                NULL) != 0) {
                    break;
                }
                continue;
            }

            switch (selector % 10) {
            case 0:
                choice = select_from_list(colorspaces,
                                          sizeof(colorspaces) /
                                          sizeof(colorspaces[0]),
                                          seed);
                if (push_option(argv, owned, &argc, &owned_count, 'X',
                                choice) != 0) {
                    i = option_count;
                }
                break;
            case 1:
                choice = select_from_list(colorspaces,
                                          sizeof(colorspaces) /
                                          sizeof(colorspaces[0]),
                                          seed);
                if (push_option(argv, owned, &argc, &owned_count, 'W',
                                choice) != 0) {
                    i = option_count;
                }
                break;
            case 2:
                choice = select_from_list(colorspaces,
                                          sizeof(colorspaces) /
                                          sizeof(colorspaces[0]),
                                          seed);
                if (push_option(argv, owned, &argc, &owned_count, 'U',
                                choice) != 0) {
                    i = option_count;
                }
                break;
            case 3:
                choice = select_from_list(resample_filters,
                                          sizeof(resample_filters) /
                                          sizeof(resample_filters[0]),
                                          seed);
                if (push_option(argv, owned, &argc, &owned_count, 'r',
                                choice) != 0) {
                    i = option_count;
                }
                break;
            case 4:
                choice = select_from_list(diffusion_modes,
                                          sizeof(diffusion_modes) /
                                          sizeof(diffusion_modes[0]),
                                          seed);
                if (push_option(argv, owned, &argc, &owned_count, 'd',
                                choice) != 0) {
                    i = option_count;
                }
                break;
            case 5:
                choice = select_from_list(diffusion_scans,
                                          sizeof(diffusion_scans) /
                                          sizeof(diffusion_scans[0]),
                                          seed);
                if (push_option(argv, owned, &argc, &owned_count, 'y',
                                choice) != 0) {
                    i = option_count;
                }
                break;
            case 6:
                choice = select_from_list(diffusion_carries,
                                          sizeof(diffusion_carries) /
                                          sizeof(diffusion_carries[0]),
                                          seed);
                if (push_option(argv, owned, &argc, &owned_count, 'Y',
                                choice) != 0) {
                    i = option_count;
                }
                break;
            case 7:
                choice = select_from_list(quality_modes,
                                          sizeof(quality_modes) /
                                          sizeof(quality_modes[0]),
                                          seed);
                if (push_option(argv, owned, &argc, &owned_count, 'q',
                                choice) != 0) {
                    i = option_count;
                }
                break;
            case 8:
                choice = select_from_list(width_height_values,
                                          sizeof(width_height_values) /
                                          sizeof(width_height_values[0]),
                                          seed);
                opt = (seed & 0x1) ? 'w' : 'h';
                if (push_option(argv, owned, &argc, &owned_count, opt,
                                choice) != 0) {
                    i = option_count;
                }
                break;
            case 9:
            default:
                choice = select_from_list(palette_types,
                                          sizeof(palette_types) /
                                          sizeof(palette_types[0]),
                                          seed);
                if (push_option(argv, owned, &argc, &owned_count, 't',
                                choice) != 0) {
                    i = option_count;
                }
                break;
            }
        }

        if (fuzz_next_byte(data, size, &offset) & 0x1) {
            model = select_from_list(quantize_models,
                                     sizeof(quantize_models) /
                                     sizeof(quantize_models[0]),
                                     fuzz_next_byte(data, size, &offset));
            (void)push_option(argv, owned, &argc, &owned_count, 'Q', model);
        }

        if (fuzz_next_byte(data, size, &offset) & 0x1) {
            colors = 2U + (unsigned int)(fuzz_next_byte(data, size, &offset)
                                         % 253U);
            (void)snprintf(numbuf, sizeof(numbuf), "%u", colors);
            (void)push_option(argv, owned, &argc, &owned_count, 'p', numbuf);
        }

        if (push_argument(argv, owned, &argc, &owned_count, input_path) != 0) {
            (void)unlink(input_path);
            (void)unlink(output_path);
            release_arguments(owned, owned_count);
            return 0;
        }

        argv[argc] = NULL;

        optind = 1;
        opterr = 0;
        result = img2sixel_main(argc, argv);
        (void)result;

        (void)unlink(input_path);
        (void)unlink(output_path);
        release_arguments(owned, owned_count);
    }

    return 0;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
