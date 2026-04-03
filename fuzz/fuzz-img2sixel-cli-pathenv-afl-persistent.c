/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * AFL++ persistent harness for img2sixel path/env option coverage.
 *
 * This target concentrates on path-taking options and environment-driven
 * path behavior (invalid paths, long paths, odd prefixes, broken assignments).
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
#define FUZZ_MAX_ARG_TEXT 512u
#define FUZZ_MAX_PATH_TEXT 8192u

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

static char const *g_toggle_env_keys[] = {
    "SIXEL_OPTION_PREFIX_SUGGESTIONS",
    "SIXEL_OPTION_FUZZY_SUGGESTIONS",
    "SIXEL_OPTION_PATH_SUGGESTIONS",
    "SIXEL_LOADER_OSC11_BG_QUERY",
    "SIXEL_ANIMATION_HIDE_CURSOR"
};

static char const *g_path_env_keys[] = {
    "TMPDIR",
    "HOME",
    "XDG_CACHE_HOME",
    "XDG_CONFIG_HOME"
};

static char const *g_toggle_values[] = {
    "",
    "0",
    "1",
    "off",
    "on",
    "auto",
    "garbage"
};

static char const *g_map_prefixes[] = {
    "",
    "act:",
    "pal:",
    "pal-jasc:",
    "pal-riff:",
    "gpl:",
    "file:",
    "http://"
};

static char const *g_path_templates[] = {
    "",
    "-",
    ".",
    "..",
    "/",
    "/tmp",
    "/tmp/../tmp/./x",
    "/tmp/nonexistent",
    "./relative/path",
    "../relative/path",
    "~/palette.gpl",
    "C:\\temp\\palette.act",
    "\\\\?\\C:\\broken\\path"
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
fuzz_append_concat2(char *argv[],
                    size_t *argc,
                    char storage[][FUZZ_MAX_ARG_TEXT],
                    char const *a,
                    char const *b)
{
    if (*argc >= FUZZ_MAX_ARGV - 1u) {
        return -1;
    }

    if (snprintf(storage[*argc],
                 FUZZ_MAX_ARG_TEXT,
                 "%s%s",
                 a,
                 b) >= (int)FUZZ_MAX_ARG_TEXT) {
        return -1;
    }

    argv[*argc] = storage[*argc];
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
fuzz_build_long_path(char *out,
                     size_t out_size,
                     unsigned char const *data,
                     size_t data_size,
                     size_t *cursor)
{
    size_t target;
    size_t pos;

    if (out == NULL || out_size == 0u) {
        return;
    }

    target = 128u + (size_t)(fuzz_read_byte(data, data_size, *cursor) % 6000u);
    *cursor += 1u;
    if (target >= out_size) {
        target = out_size - 1u;
    }

    pos = 0u;
    out[pos++] = '/';
    while (pos < target) {
        unsigned int b;
        char ch;

        b = fuzz_read_byte(data, data_size, *cursor);
        *cursor += 1u;
        ch = (char)('a' + (b % 26u));
        out[pos++] = ch;
        if ((pos % 23u) == 0u && pos + 1u < target) {
            out[pos++] = '/';
        }
    }
    out[pos] = '\0';
}

static void
fuzz_build_path_candidate(char *out,
                          size_t out_size,
                          unsigned char const *data,
                          size_t data_size,
                          size_t *cursor,
                          char const *existing_path)
{
    unsigned int mode;

    if (out == NULL || out_size == 0u) {
        return;
    }

    mode = fuzz_read_byte(data, data_size, *cursor) % 7u;
    *cursor += 1u;

    switch (mode) {
    case 0u: {
        char const *tmpl;

        tmpl = fuzz_pick(g_path_templates,
                         sizeof(g_path_templates) / sizeof(g_path_templates[0]),
                         data,
                         data_size,
                         cursor);
        (void)snprintf(out, out_size, "%s", tmpl);
        break;
    }
    case 1u:
        (void)snprintf(out, out_size, "%s", existing_path);
        break;
    case 2u:
        (void)snprintf(out, out_size, "%s.missing", existing_path);
        break;
    case 3u:
        (void)snprintf(out, out_size, "../%s", existing_path);
        break;
    case 4u:
        (void)snprintf(out, out_size, "%s/.././%s", existing_path, "palette");
        break;
    case 5u:
        fuzz_build_long_path(out, out_size, data, data_size, cursor);
        break;
    default: {
        size_t pos;
        size_t target;

        target = 8u + (size_t)(fuzz_read_byte(data, data_size, *cursor) % 128u);
        *cursor += 1u;
        if (target >= out_size) {
            target = out_size - 1u;
        }
        for (pos = 0u; pos < target; ++pos) {
            unsigned int b;

            b = fuzz_read_byte(data, data_size, *cursor);
            *cursor += 1u;
            if ((b % 11u) == 0u) {
                out[pos] = '/';
            } else if ((b % 13u) == 0u) {
                out[pos] = '.';
            } else {
                out[pos] = (char)('a' + (b % 26u));
            }
        }
        out[target] = '\0';
        break;
    }
    }
}

static void
fuzz_apply_path_env(unsigned char const *data,
                    size_t data_size,
                    size_t *cursor,
                    char const *anchor_path)
{
    size_t i;

    for (i = 0u; i < sizeof(g_toggle_env_keys) / sizeof(g_toggle_env_keys[0]); ++i) {
        unsigned int mode;

        mode = fuzz_read_byte(data, data_size, *cursor);
        *cursor += 1u;

        if ((mode & 3u) == 0u) {
            (void)unsetenv(g_toggle_env_keys[i]);
        } else {
            char const *value;

            value = g_toggle_values[
                mode % (sizeof(g_toggle_values) / sizeof(g_toggle_values[0]))];
            (void)setenv(g_toggle_env_keys[i], value, 1);
        }
    }

    for (i = 0u; i < sizeof(g_path_env_keys) / sizeof(g_path_env_keys[0]); ++i) {
        char path_value[FUZZ_MAX_PATH_TEXT];

        if ((fuzz_read_byte(data, data_size, *cursor) & 3u) == 0u) {
            *cursor += 1u;
            (void)unsetenv(g_path_env_keys[i]);
            continue;
        }

        fuzz_build_path_candidate(path_value,
                                  sizeof(path_value),
                                  data,
                                  data_size,
                                  cursor,
                                  anchor_path);
        (void)setenv(g_path_env_keys[i], path_value, 1);
    }
}

static void
fuzz_write_palette_seed(int palette_fd,
                        unsigned char const *data,
                        size_t data_size,
                        size_t *cursor)
{
    unsigned int mode;

    mode = fuzz_read_byte(data, data_size, *cursor);
    *cursor += 1u;

    if ((mode % 4u) == 0u) {
        static char const gpl_seed[] =
            "GIMP Palette\n"
            "Name: fuzz\n"
            "Columns: 2\n"
            "#\n"
            "0 0 0 black\n"
            "255 255 255 white\n";
        (void)fuzz_overwrite_file(palette_fd,
                                  (unsigned char const *)gpl_seed,
                                  sizeof(gpl_seed) - 1u);
        return;
    }

    if ((mode % 4u) == 1u) {
        static char const pal_seed[] =
            "JASC-PAL\n"
            "0100\n"
            "2\n"
            "0 0 0\n"
            "255 255 255\n";
        (void)fuzz_overwrite_file(palette_fd,
                                  (unsigned char const *)pal_seed,
                                  sizeof(pal_seed) - 1u);
        return;
    }

    if (data_size > 0u) {
        size_t payload_offset;
        size_t payload_size;

        payload_offset = *cursor;
        if (payload_offset > data_size) {
            payload_offset = data_size;
        }
        payload_size = data_size - payload_offset;
        if (payload_size > 8192u) {
            payload_size = 8192u;
        }
        if (payload_size > 0u) {
            (void)fuzz_overwrite_file(palette_fd,
                                      data + payload_offset,
                                      payload_size);
            return;
        }
    }

    {
        static unsigned char const fallback_seed[] = {
            0x00u, 0x00u, 0x00u, 0xffu, 0xffu, 0xffu
        };
        (void)fuzz_overwrite_file(palette_fd,
                                  fallback_seed,
                                  sizeof(fallback_seed));
    }
}

static void
fuzz_build_and_run(unsigned char const *data,
                   size_t data_size,
                   char const *input_path,
                   char const *palette_path,
                   char const *palette_out_path)
{
    char *argv[FUZZ_MAX_ARGV];
    char storage[FUZZ_MAX_ARGV][FUZZ_MAX_ARG_TEXT];
    char map_arg[FUZZ_MAX_PATH_TEXT];
    char map_arg_alt[FUZZ_MAX_PATH_TEXT];
    char map_out_arg[FUZZ_MAX_PATH_TEXT];
    char input_arg[FUZZ_MAX_PATH_TEXT];
    char output_arg[FUZZ_MAX_PATH_TEXT];
    size_t argc;
    size_t cursor;
    char const *map_prefix;
    char const *env_toggle_value;

    argc = 0u;
    cursor = 0u;

    fuzz_apply_path_env(data, data_size, &cursor, palette_path);

    map_prefix = fuzz_pick(g_map_prefixes,
                           sizeof(g_map_prefixes) / sizeof(g_map_prefixes[0]),
                           data,
                           data_size,
                           &cursor);

    fuzz_build_path_candidate(map_arg,
                              sizeof(map_arg),
                              data,
                              data_size,
                              &cursor,
                              palette_path);
    fuzz_build_path_candidate(map_arg_alt,
                              sizeof(map_arg_alt),
                              data,
                              data_size,
                              &cursor,
                              palette_path);
    fuzz_build_path_candidate(map_out_arg,
                              sizeof(map_out_arg),
                              data,
                              data_size,
                              &cursor,
                              palette_out_path);
    fuzz_build_path_candidate(input_arg,
                              sizeof(input_arg),
                              data,
                              data_size,
                              &cursor,
                              input_path);
    fuzz_build_path_candidate(output_arg,
                              sizeof(output_arg),
                              data,
                              data_size,
                              &cursor,
                              "/tmp/libsixel-afl-output");

    if ((fuzz_read_byte(data, data_size, cursor) & 3u) != 0u) {
        cursor += 1u;
        (void)snprintf(input_arg, sizeof(input_arg), "%s", input_path);
    }

    if (fuzz_append_const(argv, &argc, "img2sixel") != 0 ||
        fuzz_append_const(argv, &argc, "-o") != 0 ||
        fuzz_append_const(argv, &argc, "/dev/null") != 0) {
        return;
    }

    if (fuzz_append_const(argv, &argc, "--mapfile") != 0 ||
        fuzz_append_concat2(argv, &argc, storage, map_prefix, map_arg) != 0 ||
        fuzz_append_const(argv, &argc, "-m") != 0 ||
        fuzz_append_const(argv, &argc, map_arg_alt) != 0) {
        return;
    }

    if (fuzz_append_const(argv, &argc, "--mapfile-output") != 0 ||
        fuzz_append_concat3(argv,
                            &argc,
                            storage,
                            "gpl:",
                            map_out_arg,
                            "") != 0) {
        return;
    }

    if ((fuzz_read_byte(data, data_size, cursor) & 1u) != 0u) {
        cursor += 1u;
        if (fuzz_append_const(argv, &argc, "-o") != 0 ||
            fuzz_append_const(argv, &argc, output_arg) != 0) {
            return;
        }
    }

    env_toggle_value = fuzz_pick(g_toggle_values,
                                 sizeof(g_toggle_values) / sizeof(g_toggle_values[0]),
                                 data,
                                 data_size,
                                 &cursor);
    if (fuzz_append_concat4(argv,
                            &argc,
                            storage,
                            "--env=SIXEL_OPTION_PATH_SUGGESTIONS=",
                            env_toggle_value,
                            "",
                            "") != 0) {
        return;
    }

    env_toggle_value = fuzz_pick(g_toggle_values,
                                 sizeof(g_toggle_values) / sizeof(g_toggle_values[0]),
                                 data,
                                 data_size,
                                 &cursor);
    if (fuzz_append_concat4(argv,
                            &argc,
                            storage,
                            "--env=SIXEL_OPTION_PREFIX_SUGGESTIONS=",
                            env_toggle_value,
                            "",
                            "") != 0) {
        return;
    }

    if ((fuzz_read_byte(data, data_size, cursor) & 3u) == 0u) {
        cursor += 1u;
        if (fuzz_append_const(argv,
                              &argc,
                              "--env=BROKEN_ASSIGNMENT_WITHOUT_EQUALS") != 0) {
            return;
        }
    }

    if (fuzz_append_const(argv, &argc, "--") != 0 ||
        fuzz_append_const(argv, &argc, input_arg) != 0) {
        return;
    }

    argv[argc] = NULL;
    (void)img2sixel_main((int)argc, argv);
}

int
main(void)
{
    int input_fd;
    int palette_fd;
    int palette_out_fd;
    char input_path[PATH_MAX];
    char palette_path[PATH_MAX];
    char palette_out_path[PATH_MAX];

    fuzz_silence_stdio();

    input_fd = fuzz_create_tempfile(input_path,
                                    sizeof(input_path),
                                    "libsixel-afl-cli-pathenv-input");
    if (input_fd < 0) {
        return 1;
    }

    palette_fd = fuzz_create_tempfile(palette_path,
                                      sizeof(palette_path),
                                      "libsixel-afl-cli-pathenv-palette");
    if (palette_fd < 0) {
        (void)close(input_fd);
        (void)unlink(input_path);
        return 1;
    }

    palette_out_fd = fuzz_create_tempfile(palette_out_path,
                                          sizeof(palette_out_path),
                                          "libsixel-afl-cli-pathenv-out");
    if (palette_out_fd < 0) {
        (void)close(input_fd);
        (void)close(palette_fd);
        (void)unlink(input_path);
        (void)unlink(palette_path);
        return 1;
    }
    (void)close(palette_out_fd);

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
        size_t palette_cursor;

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

        palette_cursor = 0u;
        fuzz_write_palette_seed(palette_fd, data, data_size, &palette_cursor);

        fuzz_build_and_run(data,
                           data_size,
                           input_path,
                           palette_path,
                           palette_out_path);
    }
#if defined(__clang__)
# pragma clang diagnostic pop
#endif

    (void)close(input_fd);
    (void)close(palette_fd);
    (void)unlink(input_path);
    (void)unlink(palette_path);
    (void)unlink(palette_out_path);
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
