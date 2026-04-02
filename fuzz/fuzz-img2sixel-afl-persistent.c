/*
 * SPDX-License-Identifier: MIT
 *
 * AFL++ persistent-mode harness for img2sixel-style option and image fuzzing.
 *
 * The first 8 bytes steer option choices. Remaining bytes are treated as
 * image payload and written to a reusable temporary file so loader paths are
 * exercised through sixel_encoder_encode().
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

#define FUZZ_OPTION_BYTES 8u
#define FUZZ_MAX_INPUT_BYTES (1024u * 1024u)

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
fuzz_setopt(sixel_encoder_t *encoder,
            int opt,
            char const *optarg)
{
    (void)sixel_encoder_setopt(encoder, opt, optarg);
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
                     size_t path_size)
{
    char const *tmpdir;
    int fd;

    tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = "/tmp";
    }

    if (snprintf(path,
                 path_size,
                 "%s/libsixel-afl-img2sixel-XXXXXX",
                 tmpdir) >= (int)path_size) {
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
                 "%s",
                 "/tmp/libsixel-afl-img2sixel-XXXXXX") >= (int)path_size) {
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

static void
fuzz_apply_mapped_options(sixel_encoder_t *encoder,
                          unsigned char const *data,
                          size_t data_size)
{
    static char const *quality_values[] = {
        "auto", "low", "high", "full"
    };
    static char const *diffusion_values[] = {
        "auto", "none", "fs", "atkinson"
    };
    static char const *palette_values[] = {
        "auto", "rgb", "hls", "rgb"
    };
    static char const *resampling_values[] = {
        "nearest", "bilinear", "bicubic", "lanczos3", "lanczos4"
    };
    static char const *loop_values[] = {
        "auto", "force", "disable"
    };
    static char const *quantize_values[] = {
        "auto", "kmeans"
    };
    static char const *lookup_values[] = {
        "auto", "eytzinger", "none"
    };
    unsigned int b0;
    unsigned int b1;
    unsigned int b2;
    unsigned int b3;
    unsigned int b4;
    unsigned int b5;
    unsigned int b6;
    unsigned int b7;
    int colors;
    int width;
    int height;
    char number_buffer[32];

    b0 = fuzz_read_byte(data, data_size, 0u);
    b1 = fuzz_read_byte(data, data_size, 1u);
    b2 = fuzz_read_byte(data, data_size, 2u);
    b3 = fuzz_read_byte(data, data_size, 3u);
    b4 = fuzz_read_byte(data, data_size, 4u);
    b5 = fuzz_read_byte(data, data_size, 5u);
    b6 = fuzz_read_byte(data, data_size, 6u);
    b7 = fuzz_read_byte(data, data_size, 7u);

    fuzz_setopt(encoder, 'o', "/dev/null");

    if ((b0 % 2u) == 0u) {
        fuzz_setopt(encoder, 'e', NULL);
    }
    /*
     * Avoid '-D' (pipe mode): it intentionally reloads input streams and
     * can trap fuzz iterations in long-running loops.
     */
    if ((b1 % 2u) == 0u) {
        fuzz_setopt(encoder, 'i', NULL);
    }
    if ((b2 % 2u) == 0u) {
        fuzz_setopt(encoder, 'g', NULL);
    }
    if ((b4 % 2u) == 0u) {
        fuzz_setopt(encoder, 'S', NULL);
    }

    if ((b3 % 3u) == 0u) {
        fuzz_setopt(encoder, 'q', quality_values[b0 % 4u]);
        fuzz_setopt(encoder, 'd', diffusion_values[b1 % 4u]);
        fuzz_setopt(encoder, 't', palette_values[b2 % 4u]);
        fuzz_setopt(encoder, 'r', resampling_values[b3 % 5u]);
    } else if ((b3 % 3u) == 1u) {
        fuzz_setopt(encoder, 'r', resampling_values[b3 % 5u]);
        fuzz_setopt(encoder, 't', palette_values[b2 % 4u]);
        fuzz_setopt(encoder, 'd', diffusion_values[b1 % 4u]);
        fuzz_setopt(encoder, 'q', quality_values[b0 % 4u]);
    } else {
        fuzz_setopt(encoder, 't', palette_values[b2 % 4u]);
        fuzz_setopt(encoder, 'q', quality_values[b0 % 4u]);
        fuzz_setopt(encoder, 'd', diffusion_values[b1 % 4u]);
        fuzz_setopt(encoder, 'r', resampling_values[b3 % 5u]);
    }

    fuzz_setopt(encoder, 'l', loop_values[b4 % 3u]);
    fuzz_setopt(encoder, 'Q', quantize_values[b5 % 2u]);
    fuzz_setopt(encoder, '~', lookup_values[b6 % 3u]);

    colors = 2 + (int)(b5 % 255u);
    width = 1 + (int)(b6 % 160u);
    height = 1 + (int)(b7 % 120u);

    (void)snprintf(number_buffer, sizeof(number_buffer), "%d", colors);
    fuzz_setopt(encoder, 'p', number_buffer);

    (void)snprintf(number_buffer, sizeof(number_buffer), "%d", width);
    fuzz_setopt(encoder, 'w', number_buffer);

    (void)snprintf(number_buffer, sizeof(number_buffer), "%d", height);
    fuzz_setopt(encoder, 'h', number_buffer);
}

static void
fuzz_one(unsigned char const *data,
         size_t data_size,
         int temp_fd,
         char const *temp_path)
{
    sixel_encoder_t *encoder;
    unsigned char const *payload;
    size_t payload_size;
    SIXELSTATUS status;

    payload = data;
    payload_size = data_size;
    if (data_size > FUZZ_OPTION_BYTES) {
        payload = data + FUZZ_OPTION_BYTES;
        payload_size = data_size - FUZZ_OPTION_BYTES;
    }

    if (fuzz_overwrite_file(temp_fd, payload, payload_size) != 0) {
        return;
    }

    encoder = NULL;
    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status) || encoder == NULL) {
        return;
    }

    fuzz_apply_mapped_options(encoder, data, data_size);
    (void)sixel_encoder_encode(encoder, temp_path);
    sixel_encoder_unref(encoder);
}

int
main(void)
{
    int temp_fd;
    char temp_path[PATH_MAX];

    fuzz_disable_slow_paths();
    fuzz_silence_stdio();

    temp_fd = fuzz_create_tempfile(temp_path, sizeof(temp_path));
    if (temp_fd < 0) {
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

        data = __AFL_FUZZ_TESTCASE_BUF;
        data_size = (size_t)__AFL_FUZZ_TESTCASE_LEN;
        if (data_size > FUZZ_MAX_INPUT_BYTES) {
            data_size = FUZZ_MAX_INPUT_BYTES;
        }

        fuzz_one(data, data_size, temp_fd, temp_path);
    }
#if defined(__clang__)
# pragma clang diagnostic pop
#endif

    (void)close(temp_fd);
    (void)unlink(temp_path);

    return 0;
}
