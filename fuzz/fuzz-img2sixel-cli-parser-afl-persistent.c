/*
 * SPDX-License-Identifier: MIT
 *
 * AFL++ persistent harness for img2sixel CLI parser coverage.
 *
 * This target focuses on getopt/long-option parsing paths by synthesizing
 * argv tokens from fuzz bytes and then dispatching into img2sixel_main().
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
#define FUZZ_MAX_OPTION_TOKENS 36u
#define FUZZ_MAX_ARGV 96u
#define FUZZ_MAX_ARG_TEXT 192u

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

typedef struct {
    char const *short_name;
    char const *long_name;
    int has_argument;
} fuzz_cli_option_t;

static fuzz_cli_option_t const g_cli_options[] = {
    { "-p", "--colors", 1 },
    { "-w", "--width", 1 },
    { "-h", "--height", 1 },
    { "-q", "--quality", 1 },
    { "-d", "--diffusion", 1 },
    { "-t", "--palette-type", 1 },
    { "-r", "--resampling", 1 },
    { "-Q", "--quantize-model", 1 },
    { "-F", "--final-merge", 1 },
    { "-l", "--loop-control", 1 },
    { "-~", "--lookup-policy", 1 },
    { "-X", "--clustering-colorspace", 1 },
    { "-W", "--working-colorspace", 1 },
    { "-U", "--output-colorspace", 1 },
    { "-L", "--loaders", 1 },
    { "-E", "--encode-policy", 1 },
    { "-B", "--bgcolor", 1 },
    { "-m", "--mapfile", 1 },
    { "-M", "--mapfile-output", 1 },
    { "-T", "--start-frame", 1 },
    { "-n", "--macro-number", 1 },
    { "-@", "--drcs", 1 },
    { "-%", "--env", 1 },
    { "-=", "--threads", 1 },
    { "-.", "--precision", 1 },
    { "-e", "--monochrome", 0 },
    { "-i", "--invert", 0 },
    { "-I", "--high-color", 0 },
    { "-g", "--ignore-delay", 0 },
    { "-S", "--static", 0 },
    { "-u", "--use-macro", 0 },
    { "-O", "--ormode", 0 },
    { "-7", "--7bit-mode", 0 },
    { "-8", "--8bit-mode", 0 },
    { "-R", "--gri-limit", 0 },
    { "-H", "--help", 0 },
    { "-V", "--version", 0 }
};

static char const *g_value_pool[] = {
    "",
    "0",
    "1",
    "2",
    "255",
    "256",
    "-1",
    "2147483647",
    "4294967295",
    "auto",
    "none",
    "low",
    "high",
    "full",
    "rgb",
    "hls",
    "nearest",
    "lanczos4",
    "kmeans:i=none:t=0.01",
    "kmeans:i=bad:t=-1",
    "-10x-10+0+0",
    "99999%",
    "0px",
    "1024c",
    "rgb:00/00/00",
    "#fff",
    "#ffffffff",
    "builtin:orientation=off",
    "builtin:cms_engine=none",
    "SIXEL_OPTION_PREFIX_SUGGESTIONS=0",
    "BROKEN_ENV_ASSIGNMENT"
};

static char const *g_unknown_pool[] = {
    "-Z",
    "--unknown",
    "--colorspace",
    "--diff=maybe",
    "--mapfile",
    "--",
    "---"
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

static void
fuzz_disable_slow_paths(void)
{
    (void)setenv("SIXEL_LOADER_OSC11_BG_QUERY", "0", 1);
    (void)setenv("SIXEL_ANIMATION_HIDE_CURSOR", "0", 1);
    (void)setenv("SIXEL_OPTION_PREFIX_SUGGESTIONS", "0", 1);
    (void)setenv("SIXEL_OPTION_FUZZY_SUGGESTIONS", "0", 1);
    (void)setenv("SIXEL_OPTION_PATH_SUGGESTIONS", "0", 1);
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
fuzz_append_concat_eq(char *argv[],
                      size_t *argc,
                      char storage[][FUZZ_MAX_ARG_TEXT],
                      char const *lhs,
                      char const *rhs)
{
    if (*argc >= FUZZ_MAX_ARGV - 1u) {
        return -1;
    }

    if (snprintf(storage[*argc],
                 FUZZ_MAX_ARG_TEXT,
                 "%s=%s",
                 lhs,
                 rhs) >= (int)FUZZ_MAX_ARG_TEXT) {
        return -1;
    }

    argv[*argc] = storage[*argc];
    *argc += 1u;
    return 0;
}

static int
fuzz_append_concat_raw(char *argv[],
                       size_t *argc,
                       char storage[][FUZZ_MAX_ARG_TEXT],
                       char const *lhs,
                       char const *rhs)
{
    if (*argc >= FUZZ_MAX_ARGV - 1u) {
        return -1;
    }

    if (snprintf(storage[*argc],
                 FUZZ_MAX_ARG_TEXT,
                 "%s%s",
                 lhs,
                 rhs) >= (int)FUZZ_MAX_ARG_TEXT) {
        return -1;
    }

    argv[*argc] = storage[*argc];
    *argc += 1u;
    return 0;
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
    size_t limit;
    size_t index;

    argc = 0u;
    cursor = 0u;

    if (fuzz_append_const(argv, &argc, "img2sixel") != 0 ||
        fuzz_append_const(argv, &argc, "-o") != 0 ||
        fuzz_append_const(argv, &argc, "/dev/null") != 0) {
        return;
    }

    limit = 1u + (size_t)(fuzz_read_byte(data, data_size, cursor) %
                          FUZZ_MAX_OPTION_TOKENS);
    cursor += 1u;

    for (index = 0u; index < limit && argc + 3u < FUZZ_MAX_ARGV - 1u; ++index) {
        fuzz_cli_option_t const *option;
        char const *value;
        unsigned int choice;
        unsigned int style;

        choice = fuzz_read_byte(data, data_size, cursor);
        cursor += 1u;

        if ((choice % 8u) == 7u) {
            char const *unknown;

            unknown = g_unknown_pool[
                fuzz_read_byte(data, data_size, cursor)
                % (sizeof(g_unknown_pool) / sizeof(g_unknown_pool[0]))];
            cursor += 1u;

            if (fuzz_append_const(argv, &argc, unknown) != 0) {
                break;
            }
            continue;
        }

        option = &g_cli_options[
            choice % (sizeof(g_cli_options) / sizeof(g_cli_options[0]))];
        style = fuzz_read_byte(data, data_size, cursor) % 3u;
        cursor += 1u;

        if (!option->has_argument) {
            if (style == 0u) {
                if (fuzz_append_const(argv, &argc, option->short_name) != 0) {
                    break;
                }
            } else {
                if (fuzz_append_const(argv, &argc, option->long_name) != 0) {
                    break;
                }
            }
            continue;
        }

        value = g_value_pool[
            fuzz_read_byte(data, data_size, cursor)
            % (sizeof(g_value_pool) / sizeof(g_value_pool[0]))];
        cursor += 1u;

        if (style == 2u) {
            if (fuzz_append_concat_eq(argv,
                                      &argc,
                                      storage,
                                      option->long_name,
                                      value) != 0) {
                break;
            }
            continue;
        }

        if (style == 1u) {
            if (fuzz_append_const(argv, &argc, option->long_name) != 0 ||
                fuzz_append_const(argv, &argc, value) != 0) {
                break;
            }
            continue;
        }

        if ((option->short_name[0] == '-') &&
            (option->short_name[1] != '\0') &&
            (option->short_name[2] == '\0') &&
            ((fuzz_read_byte(data, data_size, cursor) & 1u) != 0u)) {
            cursor += 1u;
            if (fuzz_append_concat_raw(argv,
                                       &argc,
                                       storage,
                                       option->short_name,
                                       value) != 0) {
                break;
            }
            continue;
        }

        if (fuzz_append_const(argv, &argc, option->short_name) != 0 ||
            fuzz_append_const(argv, &argc, value) != 0) {
            break;
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

    fuzz_disable_slow_paths();
    fuzz_silence_stdio();

    input_fd = fuzz_create_tempfile(input_path,
                                    sizeof(input_path),
                                    "libsixel-afl-cli-parser-input");
    if (input_fd < 0) {
        return 1;
    }

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
        if (payload_size > 65536u) {
            payload_size = 65536u;
        }

        if (fuzz_overwrite_file(input_fd, data, payload_size) != 0) {
            continue;
        }

        fuzz_build_and_run(data, data_size, input_path);
    }

    (void)close(input_fd);
    (void)unlink(input_path);
    return 0;
}
