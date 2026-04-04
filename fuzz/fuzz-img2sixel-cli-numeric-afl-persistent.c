/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * AFL++ persistent harness for img2sixel numeric boundary coverage.
 *
 * This target focuses on numeric/overflow boundary handling in CLI options,
 * option sub-values, and environment assignments.
 */

#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include <sixel.h>

#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef PATH_MAX
# define PATH_MAX 4096
#endif

#define FUZZ_MAX_INPUT_BYTES (1024u * 1024u)
#define FUZZ_MAX_ARGV 96u
#define FUZZ_MAX_ARG_TEXT 256u

#ifndef __AFL_LOOP
static unsigned char afl_fallback_buffer[FUZZ_MAX_INPUT_BYTES];
static ssize_t afl_fallback_length;

# define __AFL_LOOP(_max) \
    ((afl_fallback_length = read(STDIN_FILENO, \
                                 afl_fallback_buffer, \
                                 sizeof(afl_fallback_buffer))) > 0)
# define __AFL_FUZZ_TESTCASE_BUF afl_fallback_buffer
# define __AFL_FUZZ_TESTCASE_LEN afl_fallback_length
# define __AFL_INIT() ((void)0)
# define __AFL_FUZZ_INIT() ((void)0)
#endif

int img2sixel_main(int argc, char *argv[]);

static char const *g_env_keys[] = {
    "SIXEL_LOADER_OSC11_BG_QUERY",
    "SIXEL_ANIMATION_HIDE_CURSOR",
    "SIXEL_OPTION_PREFIX_SUGGESTIONS",
    "SIXEL_OPTION_FUZZY_SUGGESTIONS",
    "SIXEL_OPTION_PATH_SUGGESTIONS",
    "SIXEL_FLOAT32_DITHER"
};

static char const *g_numeric_values[] = {
    "",
    "0",
    "1",
    "2",
    "7",
    "8",
    "15",
    "16",
    "255",
    "256",
    "257",
    "1024",
    "4096",
    "-1",
    "-2",
    "+1",
    "2147483647",
    "2147483648",
    "4294967295",
    "4294967296",
    "-2147483648",
    "-2147483649",
    "9223372036854775807",
    "-9223372036854775808",
    "18446744073709551615",
    "999999999999999999999999999999999999",
    "0x7fffffff",
    "077777777777",
    "1e309",
    "-1e309",
    "nan",
    "inf",
    "-inf",
    "garbage"
};

static char const *g_dimension_values[] = {
    "0",
    "1",
    "2",
    "-1",
    "2147483647",
    "2147483648",
    "99999%",
    "0%",
    "100%",
    "0px",
    "1px",
    "32767c",
    "999999c",
    "1e309"
};

static char const *g_quality_values[] = {
    "auto",
    "low",
    "high",
    "full",
    "0",
    "1",
    "-1",
    "2147483647",
    "garbage"
};

static char const *g_precision_values[] = {
    "auto",
    "8bit",
    "float32",
    "0",
    "1",
    "-1",
    "999999",
    "nan",
    "garbage"
};

static char const *g_quant_inits[] = {
    "auto",
    "none",
    "pca",
    "0",
    "-1",
    "garbage"
};

static char const *g_quant_thresholds[] = {
    "0",
    "0.0",
    "0.000001",
    "0.5",
    "1.0",
    "2.0",
    "-1",
    "1e309",
    "nan",
    "inf",
    "garbage"
};

static unsigned int
fuzz_read_byte(unsigned char const *data,
               size_t data_size,
               size_t index)
{
    if (index >= data_size) {
        return 0u;
    }

    return (unsigned int)data[index];
}

static char const *
fuzz_pick(char const *const *pool,
          size_t pool_size,
          unsigned char const *data,
          size_t data_size,
          size_t *cursor)
{
    unsigned int b;

    b = fuzz_read_byte(data, data_size, *cursor);
    *cursor += 1u;
    return pool[b % pool_size];
}

static void
fuzz_silence_stdio(void)
{
    int nullfd;

    nullfd = open("/dev/null", O_WRONLY);
    if (nullfd < 0) {
        return;
    }

    (void)dup2(nullfd, STDOUT_FILENO);
    (void)dup2(nullfd, STDERR_FILENO);

    if (nullfd > STDERR_FILENO) {
        (void)close(nullfd);
    }
}

static int
fuzz_create_tempfile(char *path,
                     size_t path_size,
                     char const *name_prefix)
{
    char const *tmpdir;
    int fd;

    tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = "/tmp";
    }

    if (snprintf(path,
                 path_size,
                 "%s/%s-XXXXXX",
                 tmpdir,
                 name_prefix) >= (int)path_size) {
        return -1;
    }

    fd = mkstemp(path);
    if (fd >= 0) {
        return fd;
    }

    if (strcmp(tmpdir, "/tmp") == 0) {
        return -1;
    }

    if (snprintf(path,
                 path_size,
                 "%s/%s-XXXXXX",
                 "/tmp",
                 name_prefix) >= (int)path_size) {
        return -1;
    }

    return mkstemp(path);
}

static int
fuzz_overwrite_file(int fd,
                    unsigned char const *data,
                    size_t data_size)
{
    size_t offset;

    if (ftruncate(fd, 0) != 0) {
        return -1;
    }

    if (lseek(fd, 0, SEEK_SET) < 0) {
        return -1;
    }

    offset = 0u;
    while (offset < data_size) {
        ssize_t written;

        written = write(fd, data + offset, data_size - offset);
        if (written <= 0) {
            return -1;
        }
        offset += (size_t)written;
    }

    if (lseek(fd, 0, SEEK_SET) < 0) {
        return -1;
    }

    return 0;
}

static int
fuzz_append_const(char *argv[],
                  size_t *argc,
                  char const *value)
{
    if (*argc >= FUZZ_MAX_ARGV - 1u) {
        return -1;
    }

    argv[*argc] = (char *)value;
    *argc += 1u;
    return 0;
}

static int
fuzz_append_concat3(char *argv[],
                    size_t *argc,
                    char storage[][FUZZ_MAX_ARG_TEXT],
                    char const *a,
                    char const *b,
                    char const *c)
{
    if (*argc >= FUZZ_MAX_ARGV - 1u) {
        return -1;
    }

    if (snprintf(storage[*argc],
                 FUZZ_MAX_ARG_TEXT,
                 "%s%s%s",
                 a,
                 b,
                 c) >= (int)FUZZ_MAX_ARG_TEXT) {
        return -1;
    }

    argv[*argc] = storage[*argc];
    *argc += 1u;
    return 0;
}

static int
fuzz_append_concat4(char *argv[],
                    size_t *argc,
                    char storage[][FUZZ_MAX_ARG_TEXT],
                    char const *a,
                    char const *b,
                    char const *c,
                    char const *d)
{
    if (*argc >= FUZZ_MAX_ARGV - 1u) {
        return -1;
    }

    if (snprintf(storage[*argc],
                 FUZZ_MAX_ARG_TEXT,
                 "%s%s%s%s",
                 a,
                 b,
                 c,
                 d) >= (int)FUZZ_MAX_ARG_TEXT) {
        return -1;
    }

    argv[*argc] = storage[*argc];
    *argc += 1u;
    return 0;
}

static void
fuzz_apply_numeric_env(unsigned char const *data,
                       size_t data_size,
                       size_t *cursor)
{
    size_t i;

    for (i = 0u; i < sizeof(g_env_keys) / sizeof(g_env_keys[0]); ++i) {
        unsigned int mode;

        mode = fuzz_read_byte(data, data_size, *cursor);
        *cursor += 1u;

        if ((mode & 3u) == 0u) {
            (void)unsetenv(g_env_keys[i]);
        } else {
            char const *value;

            value = g_numeric_values[
                mode % (sizeof(g_numeric_values) / sizeof(g_numeric_values[0]))];
            (void)setenv(g_env_keys[i], value, 1);
        }
    }
}

static void
fuzz_build_and_run(unsigned char const *data,
                   size_t data_size,
                   char const *input_path)
{
    char *argv[FUZZ_MAX_ARGV];
    char storage[FUZZ_MAX_ARGV][FUZZ_MAX_ARG_TEXT];
    size_t argc;
    size_t cursor;
    char const *colors;
    char const *width;
    char const *height;
    char const *threads;
    char const *macro_number;
    char const *start_frame;
    char const *drcs_number;
    char const *precision;
    char const *quality;
    char const *quant_init;
    char const *quant_threshold;

    argc = 0u;
    cursor = 0u;

    fuzz_apply_numeric_env(data, data_size, &cursor);

    colors = fuzz_pick(g_numeric_values,
                       sizeof(g_numeric_values) / sizeof(g_numeric_values[0]),
                       data,
                       data_size,
                       &cursor);
    width = fuzz_pick(g_dimension_values,
                      sizeof(g_dimension_values) / sizeof(g_dimension_values[0]),
                      data,
                      data_size,
                      &cursor);
    height = fuzz_pick(g_dimension_values,
                       sizeof(g_dimension_values) / sizeof(g_dimension_values[0]),
                       data,
                       data_size,
                       &cursor);
    threads = fuzz_pick(g_numeric_values,
                        sizeof(g_numeric_values) / sizeof(g_numeric_values[0]),
                        data,
                        data_size,
                        &cursor);
    macro_number = fuzz_pick(g_numeric_values,
                             sizeof(g_numeric_values) / sizeof(g_numeric_values[0]),
                             data,
                             data_size,
                             &cursor);
    start_frame = fuzz_pick(g_numeric_values,
                            sizeof(g_numeric_values) / sizeof(g_numeric_values[0]),
                            data,
                            data_size,
                            &cursor);
    drcs_number = fuzz_pick(g_numeric_values,
                            sizeof(g_numeric_values) / sizeof(g_numeric_values[0]),
                            data,
                            data_size,
                            &cursor);
    precision = fuzz_pick(g_precision_values,
                          sizeof(g_precision_values) / sizeof(g_precision_values[0]),
                          data,
                          data_size,
                          &cursor);
    quality = fuzz_pick(g_quality_values,
                        sizeof(g_quality_values) / sizeof(g_quality_values[0]),
                        data,
                        data_size,
                        &cursor);
    quant_init = fuzz_pick(g_quant_inits,
                           sizeof(g_quant_inits) / sizeof(g_quant_inits[0]),
                           data,
                           data_size,
                           &cursor);
    quant_threshold = fuzz_pick(g_quant_thresholds,
                                sizeof(g_quant_thresholds)
                                / sizeof(g_quant_thresholds[0]),
                                data,
                                data_size,
                                &cursor);

    if (fuzz_append_const(argv, &argc, "img2sixel") != 0 ||
        fuzz_append_const(argv, &argc, "-o") != 0 ||
        fuzz_append_const(argv, &argc, "/dev/null") != 0) {
        return;
    }

    if (fuzz_append_const(argv, &argc, "-p") != 0 ||
        fuzz_append_const(argv, &argc, colors) != 0 ||
        fuzz_append_const(argv, &argc, "-w") != 0 ||
        fuzz_append_const(argv, &argc, width) != 0 ||
        fuzz_append_const(argv, &argc, "-h") != 0 ||
        fuzz_append_const(argv, &argc, height) != 0 ||
        fuzz_append_const(argv, &argc, "-=") != 0 ||
        fuzz_append_const(argv, &argc, threads) != 0 ||
        fuzz_append_const(argv, &argc, "-n") != 0 ||
        fuzz_append_const(argv, &argc, macro_number) != 0 ||
        fuzz_append_const(argv, &argc, "-T") != 0 ||
        fuzz_append_const(argv, &argc, start_frame) != 0 ||
        fuzz_append_const(argv, &argc, "-@") != 0 ||
        fuzz_append_const(argv, &argc, drcs_number) != 0 ||
        fuzz_append_const(argv, &argc, "-.") != 0 ||
        fuzz_append_const(argv, &argc, precision) != 0 ||
        fuzz_append_const(argv, &argc, "-q") != 0 ||
        fuzz_append_const(argv, &argc, quality) != 0) {
        return;
    }

    if (fuzz_append_concat4(argv,
                            &argc,
                            storage,
                            "--quantize-model=kmeans:i=",
                            quant_init,
                            ":t=",
                            quant_threshold) != 0) {
        return;
    }

    if (fuzz_append_concat3(argv,
                            &argc,
                            storage,
                            "--threads=",
                            threads,
                            "") != 0 ||
        fuzz_append_concat3(argv,
                            &argc,
                            storage,
                            "--start-frame=",
                            start_frame,
                            "") != 0 ||
        fuzz_append_concat3(argv,
                            &argc,
                            storage,
                            "--macro-number=",
                            macro_number,
                            "") != 0) {
        return;
    }

    if (fuzz_append_concat4(argv,
                            &argc,
                            storage,
                            "--env=SIXEL_OPTION_PREFIX_SUGGESTIONS=",
                            fuzz_pick(g_numeric_values,
                                      sizeof(g_numeric_values) / sizeof(g_numeric_values[0]),
                                      data,
                                      data_size,
                                      &cursor),
                            "",
                            "") != 0 ||
        fuzz_append_concat4(argv,
                            &argc,
                            storage,
                            "--env=SIXEL_OPTION_FUZZY_SUGGESTIONS=",
                            fuzz_pick(g_numeric_values,
                                      sizeof(g_numeric_values) / sizeof(g_numeric_values[0]),
                                      data,
                                      data_size,
                                      &cursor),
                            "",
                            "") != 0) {
        return;
    }

    if ((fuzz_read_byte(data, data_size, cursor) & 3u) == 0u) {
        cursor += 1u;
        if (fuzz_append_const(argv, &argc, "--env=BROKEN_NUMERIC_ASSIGNMENT") != 0) {
            return;
        }
    }

    if (fuzz_append_const(argv, &argc, input_path) != 0) {
        return;
    }

    argv[argc] = NULL;
    (void)img2sixel_main((int)argc, argv);
}

int
main(void)
{
    int input_fd;
    char input_path[PATH_MAX];

    fuzz_silence_stdio();

    input_fd = fuzz_create_tempfile(input_path,
                                    sizeof(input_path),
                                    "libsixel-afl-cli-numeric-input");
    if (input_fd < 0) {
        return 1;
    }

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunused-variable"
# pragma clang diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
#endif
    __AFL_FUZZ_INIT();

#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif

    while (__AFL_LOOP(1000)) {
        unsigned char *data;
        size_t data_size;
        size_t payload_size;

        data = __AFL_FUZZ_TESTCASE_BUF;
        data_size = (size_t)__AFL_FUZZ_TESTCASE_LEN;
        if (data_size > FUZZ_MAX_INPUT_BYTES) {
            data_size = FUZZ_MAX_INPUT_BYTES;
        }

        payload_size = data_size;
        if (payload_size > 262144u) {
            payload_size = 262144u;
        }

        if (fuzz_overwrite_file(input_fd, data, payload_size) != 0) {
            continue;
        }

        fuzz_build_and_run(data, data_size, input_path);
    }
#if defined(__clang__)
# pragma clang diagnostic pop
#endif

    (void)close(input_fd);
    (void)unlink(input_path);
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
