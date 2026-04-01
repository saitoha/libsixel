/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <sixel.h>

#include "compat_stub.h"
#include "fuzz-loader-builtin-struct-common.h"

static void
fuzz_disable_option_suggestions(void)
{
    (void)sixel_compat_setenv("SIXEL_OPTION_PREFIX_SUGGESTIONS", "0");
    (void)sixel_compat_setenv("SIXEL_OPTION_FUZZY_SUGGESTIONS", "0");
}

static void
fuzz_build_ascii_token(fuzz_cursor_t *cursor, char *out, size_t out_size)
{
    static char const alphabet[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "._-/";

    size_t i;
    size_t token_len;
    size_t alphabet_size;

    if (out == NULL || out_size == 0u) {
        return;
    }

    alphabet_size = sizeof(alphabet) - 1u;
    token_len = (size_t)(fuzz_cursor_take_u8(cursor, 0u) % 48u);
    if (token_len + 1u > out_size) {
        token_len = out_size - 1u;
    }

    for (i = 0u; i < token_len; ++i) {
        unsigned char raw;

        raw = fuzz_cursor_take_u8(cursor, (unsigned char)('a' + (i % 26u)));
        out[i] = alphabet[(size_t)raw % alphabet_size];
    }

    if (token_len == 0u) {
        (void)snprintf(out, out_size, "seed");
        return;
    }

    out[token_len] = '\0';
}

static void
fuzz_build_mapfile_spec(fuzz_cursor_t *cursor, char *out, size_t out_size)
{
    static char const *prefixes[] = {
        "",
        "act:",
        "pal:",
        "pal-jasc:",
        "pal-riff:",
        "gpl:",
        "unknown:"
    };
    static char const *extensions[] = {
        "act",
        "pal",
        "gpl",
        "png",
        "bmp",
        "bin"
    };

    char path[320];
    char token0[96];
    char token1[96];
    char const *prefix;
    char const *extension;
    unsigned char kind;

    if (out == NULL || out_size == 0u) {
        return;
    }

    memset(path, 0, sizeof(path));
    memset(token0, 0, sizeof(token0));
    memset(token1, 0, sizeof(token1));

    fuzz_build_ascii_token(cursor, token0, sizeof(token0));
    fuzz_build_ascii_token(cursor, token1, sizeof(token1));

    prefix = prefixes[(size_t)fuzz_cursor_take_u8(cursor, 0u) %
                      (sizeof(prefixes) / sizeof(prefixes[0]))];
    extension = extensions[(size_t)fuzz_cursor_take_u8(cursor, 0u) %
                           (sizeof(extensions) / sizeof(extensions[0]))];
    kind = (unsigned char)(fuzz_cursor_take_u8(cursor, 0u) % 9u);

    switch (kind) {
    case 0u:
        (void)snprintf(path, sizeof(path), "-");
        break;
    case 1u:
        (void)snprintf(path, sizeof(path), "clipboard:sixel");
        break;
    case 2u:
        (void)snprintf(path, sizeof(path), "clipboard:png");
        break;
    case 3u:
        (void)snprintf(path, sizeof(path), "https://example.invalid/%s", token0);
        break;
    case 4u:
        (void)snprintf(path, sizeof(path), "http://127.0.0.1/%s", token0);
        break;
    case 5u:
        (void)snprintf(path, sizeof(path), "%s.%s", token0, extension);
        break;
    case 6u:
        (void)snprintf(path, sizeof(path), "./%s/%s.%s", token0, token1, extension);
        break;
    case 7u:
        (void)snprintf(path, sizeof(path), "/tmp/%s.%s", token0, extension);
        break;
    case 8u:
    default:
        path[0] = '\0';
        break;
    }

    (void)snprintf(out, out_size, "%s%s", prefix, path);
}

static void
fuzz_build_palette_output_spec(fuzz_cursor_t *cursor, char *out, size_t out_size)
{
    static char const *extensions[] = {
        "act",
        "pal",
        "gpl",
        "txt"
    };

    char token[96];
    char const *extension;
    unsigned char kind;

    if (out == NULL || out_size == 0u) {
        return;
    }

    fuzz_build_ascii_token(cursor, token, sizeof(token));
    extension = extensions[(size_t)fuzz_cursor_take_u8(cursor, 0u) %
                           (sizeof(extensions) / sizeof(extensions[0]))];
    kind = (unsigned char)(fuzz_cursor_take_u8(cursor, 0u) % 4u);

    switch (kind) {
    case 0u:
        (void)snprintf(out, out_size, "%s.%s", token, extension);
        break;
    case 1u:
        (void)snprintf(out, out_size, "/tmp/%s.%s", token, extension);
        break;
    case 2u:
        (void)snprintf(out, out_size, "-");
        break;
    case 3u:
    default:
        out[0] = '\0';
        break;
    }
}

static void
fuzz_build_colors_argument(fuzz_cursor_t *cursor, char *out, size_t out_size)
{
    static char const *samples[] = {
        "1",
        "2",
        "16",
        "256",
        "257",
        "0",
        "-1",
        "1!",
        "0001",
        "999999999",
        "12x"
    };

    unsigned char selector;

    if (out == NULL || out_size == 0u) {
        return;
    }

    selector = fuzz_cursor_take_u8(cursor, 0u);
    if ((selector & 0x01u) == 0u) {
        char const *sample;

        sample = samples[(size_t)selector % (sizeof(samples) / sizeof(samples[0]))];
        (void)snprintf(out, out_size, "%s", sample);
        return;
    }

    (void)snprintf(out,
                   out_size,
                   "%lu%s",
                   (unsigned long)fuzz_cursor_take_u16be(cursor, 1u),
                   ((selector & 0x02u) != 0u) ? "!" : "");
}

int
LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    fuzz_disable_option_suggestions();
    return 0;
}

int
LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
    enum { FUZZ_MAX_INPUT_BYTES = 256 * 1024 };

    fuzz_cursor_t cursor;
    sixel_encoder_t *encoder;
    char mapfile_spec[384];
    char mapfile_spec_second[384];
    char palette_output[192];
    char colors[64];

    if (data == NULL || size > (size_t)FUZZ_MAX_INPUT_BYTES) {
        return 0;
    }

    encoder = NULL;
    memset(mapfile_spec, 0, sizeof(mapfile_spec));
    memset(mapfile_spec_second, 0, sizeof(mapfile_spec_second));
    memset(palette_output, 0, sizeof(palette_output));
    memset(colors, 0, sizeof(colors));

    fuzz_cursor_init(&cursor, data, size);
    fuzz_build_mapfile_spec(&cursor, mapfile_spec, sizeof(mapfile_spec));
    fuzz_build_palette_output_spec(&cursor,
                                   palette_output,
                                   sizeof(palette_output));
    fuzz_build_colors_argument(&cursor, colors, sizeof(colors));

    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) || encoder == NULL) {
        return 0;
    }

    if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u) {
        (void)sixel_encoder_setopt(encoder,
                                   SIXEL_OPTFLAG_MAPFILE_OUTPUT,
                                   palette_output);
    }
    if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u) {
        (void)sixel_encoder_setopt(encoder,
                                   SIXEL_OPTFLAG_COLORS,
                                   colors);
    }

    (void)sixel_encoder_setopt(encoder,
                               SIXEL_OPTFLAG_MAPFILE,
                               mapfile_spec);

    if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u) {
        fuzz_build_mapfile_spec(&cursor,
                                mapfile_spec_second,
                                sizeof(mapfile_spec_second));
        (void)sixel_encoder_setopt(encoder,
                                   SIXEL_OPTFLAG_MAPFILE,
                                   mapfile_spec_second);
    }

    if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u) {
        fuzz_build_colors_argument(&cursor, colors, sizeof(colors));
        (void)sixel_encoder_setopt(encoder,
                                   SIXEL_OPTFLAG_COLORS,
                                   colors);
    }

    if ((fuzz_cursor_take_u8(&cursor, 0u) & 0x01u) != 0u) {
        fuzz_build_palette_output_spec(&cursor,
                                       palette_output,
                                       sizeof(palette_output));
        (void)sixel_encoder_setopt(encoder,
                                   SIXEL_OPTFLAG_MAPFILE_OUTPUT,
                                   palette_output);
    }

    sixel_encoder_unref(encoder);
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
