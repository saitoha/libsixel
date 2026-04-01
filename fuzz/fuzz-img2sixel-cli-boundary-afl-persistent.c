/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * AFL++ persistent harness for img2sixel option-boundary and env coverage.
 *
 * This target exercises boundary values for CLI options/sub-options and
 * environment variable handling (both process env and --env assignments).
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
    "SIXEL_LOADER_CMS_ENGINE",
    "SIXEL_FLOAT32_DITHER"
};

static char const *g_env_values[] = {
    "",
    "0",
    "1",
    "off",
    "on",
    "auto",
    "none",
    "builtin",
    "lcms2",
    "colorsync",
    "garbage"
};

static char const *g_colors_values[] = {
    "0", "1", "2", "16", "255", "256", "257", "-1", "2147483647"
};

static char const *g_dimension_values[] = {
    "0", "1", "2", "auto", "1%", "100%", "99999%", "0px", "1px",
    "1024c", "-1"
};

static char const *g_quality_values[] = {
    "auto", "low", "high", "full", "bad"
};

static char const *g_diffusion_values[] = {
    "auto", "none", "fs", "atkinson", "bluenoise", "lso2", "bad"
};

static char const *g_resampling_values[] = {
    "nearest", "bilinear", "bicubic", "lanczos4", "gaussian", "bad"
};

static char const *g_palette_values[] = {
    "auto", "rgb", "hls", "bad"
};

static char const *g_loop_values[] = {
    "auto", "force", "disable", "bad"
};

static char const *g_lookup_values[] = {
    "auto", "5bit", "6bit", "none", "eytzinger", "fhedt", "vptree", "bad"
};

static char const *g_thread_values[] = {
    "0", "1", "2", "8", "auto", "-1", "9999"
};

static char const *g_precision_values[] = {
    "auto", "8bit", "float32", "bad"
};

static char const *g_start_frame_values[] = {
    "0", "1", "-1", "2147483647", "-2147483648", "bad"
};

static char const *g_bg_values[] = {
    "#000", "#ffffff", "#ffffffff", "rgb:00/00/00", "rgb:ffff/ffff/ffff", "bad"
};

static char const *g_cluster_values[] = {
    "gamma", "linear", "oklab", "cielab", "din99d", "bad"
};

static char const *g_output_values[] = {
    "gamma", "linear", "smpte-c", "bad"
};

static char const *g_quant_inits[] = {
    "auto", "none", "pca", "bad"
};

static char const *g_quant_thresholds[] = {
    "0.0", "0.001", "0.5", "-1", "2.0", "nan"
};

static char const *g_loader_orientations[] = {
    "on", "off", "bad"
};

static char const *g_loader_cms_values[] = {
    "none", "auto", "builtin", "lcms2", "colorsync", "bad"
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
                    char const *prefix,
                    char const *value)
{
    if (*argc >= FUZZ_MAX_ARGV - 1u) {
        return -1;
    }

    if (snprintf(storage[*argc],
                 FUZZ_MAX_ARG_TEXT,
                 "%s%s",
                 prefix,
                 value) >= (int)FUZZ_MAX_ARG_TEXT) {
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
fuzz_apply_process_env(unsigned char const *data,
                       size_t data_size,
                       size_t *cursor)
{
    size_t index;

    for (index = 0u; index < sizeof(g_env_keys) / sizeof(g_env_keys[0]); ++index) {
        unsigned int mode;

        mode = fuzz_read_byte(data, data_size, *cursor);
        *cursor += 1u;

        if ((mode & 3u) == 0u) {
            (void)unsetenv(g_env_keys[index]);
        } else {
            char const *value;

            value = g_env_values[
                mode % (sizeof(g_env_values) / sizeof(g_env_values[0]))];
            (void)setenv(g_env_keys[index], value, 1);
        }
    }
}

static void
fuzz_write_palette_seed(int palette_fd,
                        unsigned char const *data,
                        size_t data_size,
                        size_t *cursor)
{
    static unsigned char const binary_seed[] = {
        0x00u, 0x02u, 0x00u, 0x00u, 0xffu, 0xffu
    };
    unsigned int mode;

    mode = fuzz_read_byte(data, data_size, *cursor);
    *cursor += 1u;

    if ((mode % 3u) == 0u) {
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

    if ((mode % 3u) == 1u) {
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

    (void)fuzz_overwrite_file(palette_fd, binary_seed, sizeof(binary_seed));
}

static void
fuzz_build_and_run(unsigned char const *data,
                   size_t data_size,
                   char const *input_path,
                   char const *palette_path,
                   char const *palette_out_path)
{
    static char const *map_prefixes[] = {
        "act:", "pal-jasc:", "pal-riff:", "gpl:"
    };
    char *argv[FUZZ_MAX_ARGV];
    char storage[FUZZ_MAX_ARGV][FUZZ_MAX_ARG_TEXT];
    size_t argc;
    size_t cursor;
    char const *colors;
    char const *width;
    char const *height;
    char const *quality;
    char const *diffusion;
    char const *resampling;
    char const *palette;
    char const *loop_mode;
    char const *lookup;
    char const *threads;
    char const *precision;
    char const *start_frame;
    char const *bgcolor;
    char const *cluster;
    char const *working;
    char const *output;
    char const *quant_init;
    char const *quant_threshold;
    char const *loader_orientation;
    char const *loader_cms;
    char const *map_prefix;

    argc = 0u;
    cursor = 0u;

    fuzz_apply_process_env(data, data_size, &cursor);

    colors = fuzz_pick(g_colors_values,
                       sizeof(g_colors_values) / sizeof(g_colors_values[0]),
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
    quality = fuzz_pick(g_quality_values,
                        sizeof(g_quality_values) / sizeof(g_quality_values[0]),
                        data,
                        data_size,
                        &cursor);
    diffusion = fuzz_pick(g_diffusion_values,
                          sizeof(g_diffusion_values) / sizeof(g_diffusion_values[0]),
                          data,
                          data_size,
                          &cursor);
    resampling = fuzz_pick(g_resampling_values,
                           sizeof(g_resampling_values) / sizeof(g_resampling_values[0]),
                           data,
                           data_size,
                           &cursor);
    palette = fuzz_pick(g_palette_values,
                        sizeof(g_palette_values) / sizeof(g_palette_values[0]),
                        data,
                        data_size,
                        &cursor);
    loop_mode = fuzz_pick(g_loop_values,
                          sizeof(g_loop_values) / sizeof(g_loop_values[0]),
                          data,
                          data_size,
                          &cursor);
    lookup = fuzz_pick(g_lookup_values,
                       sizeof(g_lookup_values) / sizeof(g_lookup_values[0]),
                       data,
                       data_size,
                       &cursor);
    threads = fuzz_pick(g_thread_values,
                        sizeof(g_thread_values) / sizeof(g_thread_values[0]),
                        data,
                        data_size,
                        &cursor);
    precision = fuzz_pick(g_precision_values,
                          sizeof(g_precision_values) / sizeof(g_precision_values[0]),
                          data,
                          data_size,
                          &cursor);
    start_frame = fuzz_pick(g_start_frame_values,
                            sizeof(g_start_frame_values) / sizeof(g_start_frame_values[0]),
                            data,
                            data_size,
                            &cursor);
    bgcolor = fuzz_pick(g_bg_values,
                        sizeof(g_bg_values) / sizeof(g_bg_values[0]),
                        data,
                        data_size,
                        &cursor);
    cluster = fuzz_pick(g_cluster_values,
                        sizeof(g_cluster_values) / sizeof(g_cluster_values[0]),
                        data,
                        data_size,
                        &cursor);
    working = fuzz_pick(g_cluster_values,
                        sizeof(g_cluster_values) / sizeof(g_cluster_values[0]),
                        data,
                        data_size,
                        &cursor);
    output = fuzz_pick(g_output_values,
                       sizeof(g_output_values) / sizeof(g_output_values[0]),
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
    loader_orientation = fuzz_pick(
        g_loader_orientations,
        sizeof(g_loader_orientations) / sizeof(g_loader_orientations[0]),
        data,
        data_size,
        &cursor);
    loader_cms = fuzz_pick(g_loader_cms_values,
                           sizeof(g_loader_cms_values)
                           / sizeof(g_loader_cms_values[0]),
                           data,
                           data_size,
                           &cursor);
    map_prefix = map_prefixes[
        fuzz_read_byte(data, data_size, cursor)
        % (sizeof(map_prefixes) / sizeof(map_prefixes[0]))];
    cursor += 1u;

    if (fuzz_append_const(argv, &argc, "img2sixel") != 0 ||
        fuzz_append_const(argv, &argc, "-o") != 0 ||
        fuzz_append_const(argv, &argc, "/dev/null") != 0) {
        return;
    }

    if (fuzz_append_const(argv, &argc, "-=") != 0 ||
        fuzz_append_const(argv, &argc, threads) != 0 ||
        fuzz_append_const(argv, &argc, "-.") != 0 ||
        fuzz_append_const(argv, &argc, precision) != 0 ||
        fuzz_append_const(argv, &argc, "-p") != 0 ||
        fuzz_append_const(argv, &argc, colors) != 0 ||
        fuzz_append_const(argv, &argc, "-w") != 0 ||
        fuzz_append_const(argv, &argc, width) != 0 ||
        fuzz_append_const(argv, &argc, "-h") != 0 ||
        fuzz_append_const(argv, &argc, height) != 0 ||
        fuzz_append_const(argv, &argc, "-q") != 0 ||
        fuzz_append_const(argv, &argc, quality) != 0 ||
        fuzz_append_const(argv, &argc, "-d") != 0 ||
        fuzz_append_const(argv, &argc, diffusion) != 0 ||
        fuzz_append_const(argv, &argc, "-r") != 0 ||
        fuzz_append_const(argv, &argc, resampling) != 0 ||
        fuzz_append_const(argv, &argc, "-t") != 0 ||
        fuzz_append_const(argv, &argc, palette) != 0 ||
        fuzz_append_const(argv, &argc, "-l") != 0 ||
        fuzz_append_const(argv, &argc, loop_mode) != 0 ||
        fuzz_append_const(argv, &argc, "-~") != 0 ||
        fuzz_append_const(argv, &argc, lookup) != 0 ||
        fuzz_append_const(argv, &argc, "-T") != 0 ||
        fuzz_append_const(argv, &argc, start_frame) != 0 ||
        fuzz_append_const(argv, &argc, "-X") != 0 ||
        fuzz_append_const(argv, &argc, cluster) != 0 ||
        fuzz_append_const(argv, &argc, "-W") != 0 ||
        fuzz_append_const(argv, &argc, working) != 0 ||
        fuzz_append_const(argv, &argc, "-U") != 0 ||
        fuzz_append_const(argv, &argc, output) != 0 ||
        fuzz_append_const(argv, &argc, "-B") != 0 ||
        fuzz_append_const(argv, &argc, bgcolor) != 0) {
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

    if (fuzz_append_concat4(argv,
                            &argc,
                            storage,
                            "--loaders=builtin:orientation=",
                            loader_orientation,
                            ":cms_engine=",
                            loader_cms) != 0) {
        return;
    }

    if ((fuzz_read_byte(data, data_size, cursor) & 1u) != 0u) {
        cursor += 1u;
        if (fuzz_append_const(argv, &argc, "-e") != 0 ||
            fuzz_append_const(argv, &argc, "-i") != 0) {
            return;
        }
    }

    if ((fuzz_read_byte(data, data_size, cursor) & 1u) != 0u) {
        cursor += 1u;
        if (fuzz_append_const(argv, &argc, "-I") != 0) {
            return;
        }
    }

    if ((fuzz_read_byte(data, data_size, cursor) & 1u) != 0u) {
        cursor += 1u;
        if (fuzz_append_const(argv, &argc, "-g") != 0 ||
            fuzz_append_const(argv, &argc, "-S") != 0) {
            return;
        }
    }

    if ((fuzz_read_byte(data, data_size, cursor) & 1u) != 0u) {
        cursor += 1u;
        if (fuzz_append_concat3(argv,
                                &argc,
                                storage,
                                "--mapfile=",
                                map_prefix,
                                palette_path) != 0) {
            return;
        }
    }

    if ((fuzz_read_byte(data, data_size, cursor) & 1u) != 0u) {
        cursor += 1u;
        if (fuzz_append_concat2(argv,
                                &argc,
                                storage,
                                "--mapfile-output=gpl:",
                                palette_out_path) != 0) {
            return;
        }
    }

    {
        char const *env_value;

        env_value = g_env_values[
            fuzz_read_byte(data, data_size, cursor)
            % (sizeof(g_env_values) / sizeof(g_env_values[0]))];
        cursor += 1u;
        if (fuzz_append_concat4(argv,
                                &argc,
                                storage,
                                "--env=",
                                g_env_keys[2],
                                "=",
                                env_value) != 0) {
            return;
        }
    }

    {
        char const *env_value;

        env_value = g_env_values[
            fuzz_read_byte(data, data_size, cursor)
            % (sizeof(g_env_values) / sizeof(g_env_values[0]))];
        cursor += 1u;
        if (fuzz_append_concat4(argv,
                                &argc,
                                storage,
                                "--env=",
                                g_env_keys[3],
                                "=",
                                env_value) != 0) {
            return;
        }
    }

    if ((fuzz_read_byte(data, data_size, cursor) & 3u) == 0u) {
        cursor += 1u;
        if (fuzz_append_const(argv, &argc, "--env=BROKEN_ASSIGNMENT") != 0) {
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
    int palette_fd;
    int palette_out_fd;
    char input_path[PATH_MAX];
    char palette_path[PATH_MAX];
    char palette_out_path[PATH_MAX];

    fuzz_silence_stdio();

    input_fd = fuzz_create_tempfile(input_path,
                                    sizeof(input_path),
                                    "libsixel-afl-cli-boundary-input");
    if (input_fd < 0) {
        return 1;
    }

    palette_fd = fuzz_create_tempfile(palette_path,
                                      sizeof(palette_path),
                                      "libsixel-afl-cli-boundary-palette");
    if (palette_fd < 0) {
        (void)close(input_fd);
        (void)unlink(input_path);
        return 1;
    }

    palette_out_fd = fuzz_create_tempfile(palette_out_path,
                                          sizeof(palette_out_path),
                                          "libsixel-afl-cli-boundary-out");
    if (palette_out_fd < 0) {
        (void)close(input_fd);
        (void)close(palette_fd);
        (void)unlink(input_path);
        (void)unlink(palette_path);
        return 1;
    }
    (void)close(palette_out_fd);

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
