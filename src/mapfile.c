/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_CTYPE_H
#include <ctype.h>
#endif  /* HAVE_CTYPE_H */
#if HAVE_ERRNO_H
#include <errno.h>
#endif  /* HAVE_ERRNO_H */
#if HAVE_LIMITS_H
#include <limits.h>
#endif  /* HAVE_LIMITS_H */
#if HAVE_STDIO_H
#include <stdio.h>
#endif  /* HAVE_STDIO_H */
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif  /* HAVE_STDLIB_H */
#if HAVE_STDINT_H
#include <stdint.h>
#endif  /* HAVE_STDINT_H */
#if HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif  /* HAVE_SYS_STAT_H */

/* Keep SIZE_MAX available even on strict C99 environments. */
#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

/* Keep palette mapfile reads bounded to avoid excessive memory growth. */
#define SIXEL_MAPFILE_READ_MAX_BYTES (16u * 1024u * 1024u)

#include <sixel.h>

#include "compat_stub.h"
#include "encoder.h"
#include "mapfile.h"

static int
sixel_path_has_extension(char const *path, char const *extension)
{
    size_t path_len;
    size_t ext_len;
    size_t index;

    path_len = 0u;
    ext_len = 0u;
    index = 0u;

    if (path == NULL || extension == NULL) {
        return 0;
    }

    path_len = strlen(path);
    ext_len = strlen(extension);
    if (ext_len == 0u || path_len < ext_len) {
        return 0;
    }

    for (index = 0u; index < ext_len; ++index) {
        unsigned char path_ch;
        unsigned char ext_ch;

        path_ch = (unsigned char)path[path_len - ext_len + index];
        ext_ch = (unsigned char)extension[index];
        if (tolower(path_ch) != tolower(ext_ch)) {
            return 0;
        }
    }

    return 1;
}


/*
 * Palette specification parser
 *
 *   TYPE:PATH  -> explicit format prefix
 *   PATH       -> rely on extension or heuristics
 *
 * The ASCII diagram below shows how the prefix is peeled:
 *
 *   [type] : [path]
 *    ^-- left part selects decoder/encoder when present.
 */
char const *
sixel_palette_strip_prefix(char const *spec,
                           sixel_palette_format_t *format_hint)
{
    char const *colon;
    size_t type_len;
    size_t index;
    char lowered[16];

    colon = NULL;
    type_len = 0u;
    index = 0u;

    if (format_hint != NULL) {
        *format_hint = SIXEL_PALETTE_FORMAT_NONE;
    }
    if (spec == NULL) {
        return NULL;
    }

    colon = strchr(spec, ':');
    if (colon == NULL) {
        return spec;
    }

    type_len = (size_t)(colon - spec);
    if (type_len == 0u || type_len >= sizeof(lowered)) {
        return spec;
    }

    for (index = 0u; index < type_len; ++index) {
        lowered[index] = (char)tolower((unsigned char)spec[index]);
    }
    lowered[type_len] = '\0';

    if (strcmp(lowered, "act") == 0) {
        if (format_hint != NULL) {
            *format_hint = SIXEL_PALETTE_FORMAT_ACT;
        }
        return colon + 1;
    }
    if (strcmp(lowered, "pal") == 0) {
        if (format_hint != NULL) {
            *format_hint = SIXEL_PALETTE_FORMAT_PAL_AUTO;
        }
        return colon + 1;
    }
    if (strcmp(lowered, "pal-jasc") == 0) {
        if (format_hint != NULL) {
            *format_hint = SIXEL_PALETTE_FORMAT_PAL_JASC;
        }
        return colon + 1;
    }
    if (strcmp(lowered, "pal-riff") == 0) {
        if (format_hint != NULL) {
            *format_hint = SIXEL_PALETTE_FORMAT_PAL_RIFF;
        }
        return colon + 1;
    }
    if (strcmp(lowered, "gpl") == 0) {
        if (format_hint != NULL) {
            *format_hint = SIXEL_PALETTE_FORMAT_GPL;
        }
        return colon + 1;
    }

    return spec;
}

sixel_palette_format_t
sixel_palette_format_from_extension(char const *path)
{
    if (path == NULL) {
        return SIXEL_PALETTE_FORMAT_NONE;
    }

    if (sixel_path_has_extension(path, ".act")) {
        return SIXEL_PALETTE_FORMAT_ACT;
    }
    if (sixel_path_has_extension(path, ".pal")) {
        return SIXEL_PALETTE_FORMAT_PAL_AUTO;
    }
    if (sixel_path_has_extension(path, ".gpl")) {
        return SIXEL_PALETTE_FORMAT_GPL;
    }

    return SIXEL_PALETTE_FORMAT_NONE;
}

int
sixel_path_has_any_extension(char const *path)
{
    char const *slash_forward;
#if defined(_WIN32)
    char const *slash_backward;
#endif
    char const *start;
    char const *dot;

    slash_forward = NULL;
#if defined(_WIN32)
    slash_backward = NULL;
#endif
    start = path;
    dot = NULL;

    if (path == NULL) {
        return 0;
    }

    slash_forward = strrchr(path, '/');
#if defined(_WIN32)
    slash_backward = strrchr(path, '\\');
    if (slash_backward != NULL &&
            (slash_forward == NULL || slash_backward > slash_forward)) {
        slash_forward = slash_backward;
    }
#endif
    if (slash_forward == NULL) {
        start = path;
    } else {
        start = slash_forward + 1;
    }

    dot = strrchr(start, '.');
    if (dot == NULL) {
        return 0;
    }

    if (dot[1] == '\0') {
        return 0;
    }

    return 1;
}

static int
sixel_palette_has_utf8_bom(unsigned char const *data, size_t size)
{
    if (data == NULL || size < 3u) {
        return 0;
    }
    if (data[0] == 0xefu && data[1] == 0xbbu && data[2] == 0xbfu) {
        return 1;
    }
    return 0;
}

static int
sixel_palette_tail_is_blank(char const *tail)
{
    if (tail == NULL) {
        return 0;
    }

    while (*tail == ' ' || *tail == '\t') {
        ++tail;
    }

    return *tail == '\0';
}


/*
 * Materialize palette bytes from a stream.
 *
 * The flow looks like:
 *
 *   stream --> [scratch buffer] --> [resizable heap buffer]
 *                  ^ looped read        ^ returned payload
 */
SIXELSTATUS
sixel_palette_read_stream(FILE *stream,
                          sixel_allocator_t *allocator,
                          unsigned char **pdata,
                          size_t *psize)
{
    SIXELSTATUS status;
    unsigned char *buffer;
    unsigned char *grown;
    size_t capacity;
    size_t used;
    size_t read_bytes;
    size_t needed;
    size_t new_capacity;
    unsigned char scratch[4096];

    status = SIXEL_FALSE;
    buffer = NULL;
    grown = NULL;
    capacity = 0u;
    used = 0u;
    read_bytes = 0u;
    needed = 0u;
    new_capacity = 0u;

    if (pdata == NULL || psize == NULL || stream == NULL || allocator == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_read_stream: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }

    *pdata = NULL;
    *psize = 0u;

    while (1) {
        read_bytes = fread(scratch, 1, sizeof(scratch), stream);
        if (read_bytes == 0u) {
            if (ferror(stream)) {
                sixel_helper_set_additional_message(
                    "sixel_palette_read_stream: fread() failed.");
                status = SIXEL_LIBC_ERROR;
                goto cleanup;
            }
            break;
        }

        if (used > SIZE_MAX - read_bytes) {
            sixel_helper_set_additional_message(
                "sixel_palette_read_stream: size overflow.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        needed = used + read_bytes;
        /* Hard-cap mapfile payload growth to 16 MiB. */
        if (needed > SIXEL_MAPFILE_READ_MAX_BYTES) {
            sixel_helper_set_additional_message(
                "sixel_palette_read_stream: palette input exceeds "
                "16 MiB limit.");
            status = SIXEL_BAD_INPUT;
            goto cleanup;
        }

        if (needed > capacity) {
            new_capacity = capacity;
            if (new_capacity == 0u) {
                new_capacity = 4096u;
            }
            while (needed > new_capacity) {
                if (new_capacity > SIZE_MAX / 2u) {
                    sixel_helper_set_additional_message(
                        "sixel_palette_read_stream: size overflow.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                new_capacity *= 2u;
            }

            grown = (unsigned char *)sixel_allocator_malloc(allocator,
                                                             new_capacity);
            if (grown == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_palette_read_stream: allocation failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto cleanup;
            }

            if (buffer != NULL) {
                memcpy(grown, buffer, used);
                sixel_allocator_free(allocator, buffer);
            }

            buffer = grown;
            grown = NULL;
            capacity = new_capacity;
        }

        memcpy(buffer + used, scratch, read_bytes);
        used += read_bytes;
    }

    *pdata = buffer;
    *psize = used;
    status = SIXEL_OK;
    return status;

cleanup:
    if (grown != NULL) {
        sixel_allocator_free(allocator, grown);
    }
    if (buffer != NULL) {
        sixel_allocator_free(allocator, buffer);
    }
    return status;
}


SIXELSTATUS
sixel_palette_open_read(char const *path, FILE **pstream, int *pclose)
{
    int error_value;
    char error_message[256];
    char strerror_buffer[128];
#if HAVE_SYS_STAT_H
    struct stat path_stat;
#endif

    if (pstream == NULL || pclose == NULL || path == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_open_read: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }

    error_value = 0;
    error_message[0] = '\0';

    if (strcmp(path, "-") == 0) {
        *pstream = stdin;
        *pclose = 0;
        return SIXEL_OK;
    }

#if HAVE_SYS_STAT_H
    if (stat(path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
        sixel_compat_snprintf(error_message,
                              sizeof(error_message),
                              "sixel_palette_open_read: mapfile \"%s\" "
                              "is a directory.",
                              path);
        sixel_helper_set_additional_message(error_message);
        return SIXEL_BAD_INPUT;
    }
#endif

    errno = 0;
    *pstream = sixel_compat_fopen(path, "rb");
    if (*pstream == NULL) {
        error_value = errno;
        sixel_compat_snprintf(error_message,
                              sizeof(error_message),
                              "sixel_palette_open_read: failed to open "
                              "\"%s\": %s.",
                              path,
                              sixel_compat_strerror(error_value,
                                                    strerror_buffer,
                                                    sizeof(strerror_buffer)));
        sixel_helper_set_additional_message(error_message);
        return SIXEL_LIBC_ERROR;
    }

    *pclose = 1;
    return SIXEL_OK;
}


void
sixel_palette_close_stream(FILE *stream, int close_stream)
{
    if (close_stream && stream != NULL) {
        (void) fclose(stream);
    }
}


sixel_palette_format_t
sixel_palette_guess_format(unsigned char const *data, size_t size)
{
    size_t offset;
    size_t data_size;

    offset = 0u;
    data_size = size;

    if (data == NULL || size == 0u) {
        return SIXEL_PALETTE_FORMAT_NONE;
    }

    if (size == 256u * 3u || size == 256u * 3u + 4u) {
        return SIXEL_PALETTE_FORMAT_ACT;
    }

    if (size >= 12u && memcmp(data, "RIFF", 4) == 0
            && memcmp(data + 8, "PAL ", 4) == 0) {
        return SIXEL_PALETTE_FORMAT_PAL_RIFF;
    }

    if (sixel_palette_has_utf8_bom(data, size)) {
        offset = 3u;
        data_size = size - 3u;
    }

    if (data_size >= 8u && memcmp(data + offset, "JASC-PAL", 8) == 0) {
        return SIXEL_PALETTE_FORMAT_PAL_JASC;
    }
    if (data_size >= 12u && memcmp(data + offset, "GIMP Palette", 12) == 0) {
        return SIXEL_PALETTE_FORMAT_GPL;
    }

    return SIXEL_PALETTE_FORMAT_NONE;
}


static unsigned int
sixel_palette_read_le16(unsigned char const *ptr)
{
    if (ptr == NULL) {
        return 0u;
    }
    return (unsigned int)ptr[0] | ((unsigned int)ptr[1] << 8);
}


static unsigned int
sixel_palette_read_le32(unsigned char const *ptr)
{
    if (ptr == NULL) {
        return 0u;
    }
    return ((unsigned int)ptr[0])
        | ((unsigned int)ptr[1] << 8)
        | ((unsigned int)ptr[2] << 16)
        | ((unsigned int)ptr[3] << 24);
}


/*
 * Adobe Color Table (*.act) reader
 *
 *   +-----------+---------------------------+
 *   | section   | bytes                     |
 *   +-----------+---------------------------+
 *   | palette   | 256 entries * 3 RGB bytes |
 *   | trailer   | optional count/start pair |
 *   +-----------+---------------------------+
 */
SIXELSTATUS
sixel_palette_parse_act(unsigned char const *data,
                        size_t size,
                        sixel_encoder_t *encoder,
                        sixel_dither_t **dither)
{
    SIXELSTATUS status;
    sixel_dither_t *local;
    unsigned char const *palette_start;
    unsigned char const *trailer;
    sixel_palette_t *palette_obj;
    int exported_colors;
    int start_index;

    status = SIXEL_FALSE;
    local = NULL;
    palette_start = data;
    trailer = NULL;
    palette_obj = NULL;
    exported_colors = 0;
    start_index = 0;

    if (encoder == NULL || dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_act: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (data == NULL || size < 256u * 3u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_act: truncated ACT palette.");
        return SIXEL_BAD_INPUT;
    }

    if (size == 256u * 3u) {
        exported_colors = 256;
        start_index = 0;
    } else if (size == 256u * 3u + 4u) {
        trailer = data + 256u * 3u;
        exported_colors = (int)(((unsigned int)trailer[0] << 8)
                                | (unsigned int)trailer[1]);
        start_index = (int)(((unsigned int)trailer[2] << 8)
                            | (unsigned int)trailer[3]);
    } else {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_act: invalid ACT length.");
        return SIXEL_BAD_INPUT;
    }

    if (start_index < 0 || start_index >= 256) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_act: ACT start index out of range.");
        return SIXEL_BAD_INPUT;
    }
    /*
     * Keep legacy ACT behavior for count 0 (means 256), but reject
     * explicit values above the 8-bit palette limit.
     */
    if (exported_colors <= 0) {
        exported_colors = 256;
    } else if (exported_colors > 256) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_act: invalid ACT color count.");
        return SIXEL_BAD_INPUT;
    }
    if (start_index + exported_colors > 256) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_act: ACT palette exceeds 256 slots.");
        return SIXEL_BAD_INPUT;
    }

    status = sixel_dither_new(&local, exported_colors, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    sixel_dither_set_lut_policy(local, encoder->lut_policy);

    status = sixel_dither_get_quantized_palette(local, &palette_obj);
    if (SIXEL_FAILED(status) || palette_obj == NULL) {
        sixel_dither_unref(local);
        return status;
    }
    status = sixel_palette_set_entries(
        palette_obj,
        palette_start + (size_t)start_index * 3u,
        (unsigned int)exported_colors,
        3,
        encoder->allocator);
    sixel_palette_unref(palette_obj);
    if (SIXEL_FAILED(status)) {
        sixel_dither_unref(local);
        return status;
    }

    *dither = local;
    return SIXEL_OK;
}


SIXELSTATUS
sixel_palette_parse_pal_jasc(unsigned char const *data,
                             size_t size,
                             sixel_encoder_t *encoder,
                             sixel_dither_t **dither)
{
    SIXELSTATUS status;
    char *text;
    size_t index;
    size_t offset;
    char *cursor;
    char *line;
    char *line_end;
    int stage;
    int exported_colors;
    int parsed_colors;
    sixel_dither_t *local;
    sixel_palette_t *palette_obj;
    unsigned char *palette_buffer;
    long component;
    char *parse_end;
    int value_index;
    int values[3];
    char tail;

    status = SIXEL_FALSE;
    text = NULL;
    index = 0u;
    offset = 0u;
    cursor = NULL;
    line = NULL;
    line_end = NULL;
    stage = 0;
    exported_colors = 0;
    parsed_colors = 0;
    local = NULL;
    palette_obj = NULL;
    palette_buffer = NULL;
    component = 0;
    parse_end = NULL;
    value_index = 0;
    values[0] = 0;
    values[1] = 0;
    values[2] = 0;

    if (encoder == NULL || dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_jasc: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (data == NULL || size == 0u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_jasc: empty palette.");
        return SIXEL_BAD_INPUT;
    }
    if (size > SIZE_MAX - 1u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_jasc: size overflow.");
        return SIXEL_BAD_ALLOCATION;
    }

    text = (char *)sixel_allocator_malloc(encoder->allocator, size + 1u);
    if (text == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_jasc: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    memcpy(text, data, size);
    text[size] = '\0';

    if (sixel_palette_has_utf8_bom((unsigned char const *)text, size)) {
        offset = 3u;
    }
    cursor = text + offset;

    while (*cursor != '\0') {
        line = cursor;
        line_end = cursor;
        while (*line_end != '\0' && *line_end != '\n' && *line_end != '\r') {
            ++line_end;
        }
        if (*line_end != '\0') {
            *line_end = '\0';
            cursor = line_end + 1;
        } else {
            cursor = line_end;
        }
        while (*cursor == '\n' || *cursor == '\r') {
            ++cursor;
        }

        while (*line == ' ' || *line == '\t') {
            ++line;
        }
        index = strlen(line);
        while (index > 0u) {
            tail = line[index - 1];
            if (tail != ' ' && tail != '\t') {
                break;
            }
            line[index - 1] = '\0';
            --index;
        }
        if (*line == '\0') {
            continue;
        }
        if (*line == '#') {
            continue;
        }

        if (stage == 0) {
            if (strcmp(line, "JASC-PAL") != 0) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_pal_jasc: missing header.");
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            stage = 1;
            continue;
        }
        if (stage == 1) {
            stage = 2;
            continue;
        }
        if (stage == 2) {
            component = strtol(line, &parse_end, 10);
            /* Reject glued suffixes such as "256x" in strict mode. */
            if (parse_end == line || component <= 0L || component > 256L
                    || !sixel_palette_tail_is_blank(parse_end)) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_pal_jasc: invalid color count.");
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            exported_colors = (int)component;
            if (exported_colors <= 0) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_pal_jasc: invalid color count.");
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            palette_buffer = (unsigned char *)sixel_allocator_malloc(
                encoder->allocator,
                (size_t)exported_colors * 3u);
            if (palette_buffer == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_pal_jasc: allocation failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto cleanup;
            }
            status = sixel_dither_new(&local, exported_colors,
                                      encoder->allocator);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
            sixel_dither_set_lut_policy(local, encoder->lut_policy);
            stage = 3;
            continue;
        }

        value_index = 0;
        while (value_index < 3) {
            component = strtol(line, &parse_end, 10);
            if (parse_end == line || component < 0L || component > 255L) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_pal_jasc: invalid component.");
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            values[value_index] = (int)component;
            ++value_index;
            line = parse_end;
            while (*line == ' ' || *line == '\t') {
                ++line;
            }
        }
        /* A JASC color row must be exactly 3 numeric components. */
        if (*line != '\0') {
            sixel_helper_set_additional_message(
                "sixel_palette_parse_pal_jasc: invalid component.");
            status = SIXEL_BAD_INPUT;
            goto cleanup;
        }

        if (parsed_colors >= exported_colors) {
            sixel_helper_set_additional_message(
                "sixel_palette_parse_pal_jasc: excess entries.");
            status = SIXEL_BAD_INPUT;
            goto cleanup;
        }

        palette_buffer[parsed_colors * 3 + 0] =
            (unsigned char)values[0];
        palette_buffer[parsed_colors * 3 + 1] =
            (unsigned char)values[1];
        palette_buffer[parsed_colors * 3 + 2] =
            (unsigned char)values[2];
        ++parsed_colors;
    }

    if (stage < 3) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_jasc: incomplete header.");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }
    if (parsed_colors != exported_colors) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_jasc: color count mismatch.");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }

    status = sixel_dither_get_quantized_palette(local, &palette_obj);
    if (SIXEL_FAILED(status) || palette_obj == NULL) {
        goto cleanup;
    }
    status = sixel_palette_set_entries(palette_obj,
                                       palette_buffer,
                                       (unsigned int)exported_colors,
                                       3,
                                       encoder->allocator);
    sixel_palette_unref(palette_obj);
    palette_obj = NULL;
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    *dither = local;
    status = SIXEL_OK;

cleanup:
    if (palette_obj != NULL) {
        sixel_palette_unref(palette_obj);
    }
    if (SIXEL_FAILED(status) && local != NULL) {
        sixel_dither_unref(local);
    }
    if (palette_buffer != NULL) {
        sixel_allocator_free(encoder->allocator, palette_buffer);
    }
    if (text != NULL) {
        sixel_allocator_free(encoder->allocator, text);
    }
    return status;
}


SIXELSTATUS
sixel_palette_parse_pal_riff(unsigned char const *data,
                             size_t size,
                             sixel_encoder_t *encoder,
                             sixel_dither_t **dither)
{
    SIXELSTATUS status;
    size_t offset;
    size_t chunk_size;
    sixel_dither_t *local;
    sixel_palette_t *palette_obj;
    unsigned char const *chunk;
    unsigned char *palette_buffer;
    unsigned int entry_count;
    unsigned int version;
    unsigned int index;
    size_t palette_offset;
    size_t remaining;
    size_t chunk_payload_limit;
    size_t chunk_padded_size;
    size_t next_offset;

    status = SIXEL_FALSE;
    offset = 0u;
    chunk_size = 0u;
    local = NULL;
    chunk = NULL;
    palette_obj = NULL;
    palette_buffer = NULL;
    entry_count = 0u;
    version = 0u;
    index = 0u;
    palette_offset = 0u;
    remaining = 0u;
    chunk_payload_limit = 0u;
    chunk_padded_size = 0u;
    next_offset = 0u;

    if (encoder == NULL || dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (data == NULL || size < 12u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: truncated palette.");
        return SIXEL_BAD_INPUT;
    }
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "PAL ", 4) != 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: missing RIFF header.");
        return SIXEL_BAD_INPUT;
    }

    offset = 12u;
    /*
     * Keep chunk traversal overflow-safe by checking remaining bytes
     * with subtraction before any additive offset arithmetic.
     */
    while (offset <= size && size - offset >= 8u) {
        remaining = size - offset;
        chunk = data + offset;
        chunk_size = (size_t)sixel_palette_read_le32(chunk + 4);
        chunk_payload_limit = remaining - 8u;
        if (chunk_size > chunk_payload_limit) {
            sixel_helper_set_additional_message(
                "sixel_palette_parse_pal_riff: chunk extends past end.");
            return SIXEL_BAD_INPUT;
        }
        if (memcmp(chunk, "data", 4) == 0) {
            break;
        }
        chunk_padded_size = chunk_size;
        if ((chunk_padded_size & 1u) != 0u) {
            if (chunk_padded_size == SIZE_MAX) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_pal_riff: size overflow.");
                return SIXEL_BAD_ALLOCATION;
            }
            ++chunk_padded_size;
        }
        if (chunk_padded_size > chunk_payload_limit) {
            sixel_helper_set_additional_message(
                "sixel_palette_parse_pal_riff: chunk extends past end.");
            return SIXEL_BAD_INPUT;
        }
        if (offset > SIZE_MAX - 8u - chunk_padded_size) {
            sixel_helper_set_additional_message(
                "sixel_palette_parse_pal_riff: size overflow.");
            return SIXEL_BAD_ALLOCATION;
        }
        next_offset = offset + 8u + chunk_padded_size;
        offset = next_offset;
    }

    if (offset > size || size - offset < 8u || chunk == NULL ||
            memcmp(chunk, "data", 4) != 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: missing data chunk.");
        return SIXEL_BAD_INPUT;
    }

    if (chunk_size < 4u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: data chunk too small.");
        return SIXEL_BAD_INPUT;
    }
    version = sixel_palette_read_le16(chunk + 8);
    (void)version;
    entry_count = sixel_palette_read_le16(chunk + 10);
    if (entry_count == 0u || entry_count > 256u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: invalid entry count.");
        return SIXEL_BAD_INPUT;
    }
    if (chunk_size != 4u + (size_t)entry_count * 4u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: unexpected chunk size.");
        return SIXEL_BAD_INPUT;
    }

    status = sixel_dither_new(&local, (int)entry_count, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    sixel_dither_set_lut_policy(local, encoder->lut_policy);
    palette_buffer = (unsigned char *)sixel_allocator_malloc(
        encoder->allocator,
        (size_t)entry_count * 3u);
    if (palette_buffer == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: allocation failed.");
        sixel_dither_unref(local);
        return SIXEL_BAD_ALLOCATION;
    }
    palette_offset = 12u;
    for (index = 0u; index < entry_count; ++index) {
        palette_buffer[index * 3u + 0u] =
            chunk[palette_offset + index * 4u + 0u];
        palette_buffer[index * 3u + 1u] =
            chunk[palette_offset + index * 4u + 1u];
        palette_buffer[index * 3u + 2u] =
            chunk[palette_offset + index * 4u + 2u];
    }

    status = sixel_dither_get_quantized_palette(local, &palette_obj);
    if (SIXEL_FAILED(status) || palette_obj == NULL) {
        sixel_allocator_free(encoder->allocator, palette_buffer);
        sixel_dither_unref(local);
        return status;
    }
    status = sixel_palette_set_entries(palette_obj,
                                       palette_buffer,
                                       (unsigned int)entry_count,
                                       3,
                                       encoder->allocator);
    sixel_palette_unref(palette_obj);
    palette_obj = NULL;
    sixel_allocator_free(encoder->allocator, palette_buffer);
    palette_buffer = NULL;
    if (SIXEL_FAILED(status)) {
        sixel_dither_unref(local);
        return status;
    }

    *dither = local;
    return SIXEL_OK;
}


SIXELSTATUS
sixel_palette_parse_gpl(unsigned char const *data,
                        size_t size,
                        sixel_encoder_t *encoder,
                        sixel_dither_t **dither)
{
    SIXELSTATUS status;
    char *text;
    size_t offset;
    char *cursor;
    char *line;
    char *line_end;
    size_t index;
    int header_seen;
    int parsed_colors;
    unsigned char palette_bytes[256 * 3];
    long component;
    char *parse_end;
    int value_index;
    int values[3];
    sixel_dither_t *local;
    sixel_palette_t *palette_obj;
    char tail;

    status = SIXEL_FALSE;
    text = NULL;
    offset = 0u;
    cursor = NULL;
    line = NULL;
    line_end = NULL;
    index = 0u;
    header_seen = 0;
    parsed_colors = 0;
    component = 0;
    parse_end = NULL;
    value_index = 0;
    values[0] = 0;
    values[1] = 0;
    values[2] = 0;
    local = NULL;
    palette_obj = NULL;

    if (encoder == NULL || dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_gpl: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (data == NULL || size == 0u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_gpl: empty palette.");
        return SIXEL_BAD_INPUT;
    }
    if (size > SIZE_MAX - 1u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_gpl: size overflow.");
        return SIXEL_BAD_ALLOCATION;
    }

    text = (char *)sixel_allocator_malloc(encoder->allocator, size + 1u);
    if (text == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_gpl: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    memcpy(text, data, size);
    text[size] = '\0';

    if (sixel_palette_has_utf8_bom((unsigned char const *)text, size)) {
        offset = 3u;
    }
    cursor = text + offset;

    while (*cursor != '\0') {
        line = cursor;
        line_end = cursor;
        while (*line_end != '\0' && *line_end != '\n' && *line_end != '\r') {
            ++line_end;
        }
        if (*line_end != '\0') {
            *line_end = '\0';
            cursor = line_end + 1;
        } else {
            cursor = line_end;
        }
        while (*cursor == '\n' || *cursor == '\r') {
            ++cursor;
        }

        while (*line == ' ' || *line == '\t') {
            ++line;
        }
        index = strlen(line);
        while (index > 0u) {
            tail = line[index - 1];
            if (tail != ' ' && tail != '\t') {
                break;
            }
            line[index - 1] = '\0';
            --index;
        }
        if (*line == '\0') {
            continue;
        }
        if (*line == '#') {
            continue;
        }
        if (strncmp(line, "Name:", 5) == 0) {
            continue;
        }
        if (strncmp(line, "Columns:", 8) == 0) {
            continue;
        }

        if (!header_seen) {
            if (strcmp(line, "GIMP Palette") != 0) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_gpl: missing header.");
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            header_seen = 1;
            continue;
        }

        if (parsed_colors >= 256) {
            sixel_helper_set_additional_message(
                "sixel_palette_parse_gpl: too many colors.");
            status = SIXEL_BAD_INPUT;
            goto cleanup;
        }

        value_index = 0;
        while (value_index < 3) {
            component = strtol(line, &parse_end, 10);
            if (parse_end == line || component < 0L || component > 255L) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_gpl: invalid component.");
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            /*
             * Keep GPL compatibility with optional trailing labels,
             * but reject numeric tokens with glued suffixes like "0x".
             */
            if (*parse_end != '\0' && *parse_end != ' '
                    && *parse_end != '\t') {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_gpl: invalid component.");
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            values[value_index] = (int)component;
            ++value_index;
            line = parse_end;
            while (*line == ' ' || *line == '\t') {
                ++line;
            }
        }

        palette_bytes[parsed_colors * 3 + 0] =
            (unsigned char)values[0];
        palette_bytes[parsed_colors * 3 + 1] =
            (unsigned char)values[1];
        palette_bytes[parsed_colors * 3 + 2] =
            (unsigned char)values[2];
        ++parsed_colors;
    }

    if (!header_seen) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_gpl: header missing.");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }
    if (parsed_colors <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_gpl: no colors parsed.");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }

    status = sixel_dither_new(&local, parsed_colors, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    sixel_dither_set_lut_policy(local, encoder->lut_policy);
    status = sixel_dither_get_quantized_palette(local, &palette_obj);
    if (SIXEL_FAILED(status) || palette_obj == NULL) {
        goto cleanup;
    }
    status = sixel_palette_set_entries(palette_obj,
                                       palette_bytes,
                                       (unsigned int)parsed_colors,
                                       3,
                                       encoder->allocator);
    sixel_palette_unref(palette_obj);
    palette_obj = NULL;
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    *dither = local;
    status = SIXEL_OK;

cleanup:
    if (palette_obj != NULL) {
        sixel_palette_unref(palette_obj);
    }
    if (SIXEL_FAILED(status) && local != NULL) {
        sixel_dither_unref(local);
    }
    if (text != NULL) {
        sixel_allocator_free(encoder->allocator, text);
    }
    return status;
}


/*
 * Palette exporters
 *
 *   +----------+-------------------------+
 *   | format   | emission strategy       |
 *   +----------+-------------------------+
 *   | ACT      | fixed 256 entries + EOF |
 *   | PAL JASC | textual lines           |
 *   | PAL RIFF | RIFF container          |
 *   | GPL      | textual lines           |
 *   +----------+-------------------------+
 */
SIXELSTATUS
sixel_palette_write_act(FILE *stream,
                        unsigned char const *palette,
                        size_t palette_bytes,
                        int exported_colors)
{
    SIXELSTATUS status;
    unsigned char zero_pad[256 * 3];
    unsigned char trailer[4];
    size_t exported_bytes;
    size_t pad_bytes;

    status = SIXEL_FALSE;
    exported_bytes = 0u;

    if (stream == NULL || palette == NULL || exported_colors <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (exported_colors > 256) {
        exported_colors = 256;
    }

    memset(zero_pad, 0, sizeof(zero_pad));
    exported_bytes = (size_t)exported_colors * 3u;
    if (palette_bytes < exported_bytes) {
        return SIXEL_BAD_ARGUMENT;
    }
    pad_bytes = sizeof(zero_pad) - exported_bytes;

    trailer[0] = (unsigned char)(((unsigned int)exported_colors >> 8)
                                 & 0xffu);
    trailer[1] = (unsigned char)((unsigned int)exported_colors & 0xffu);
    trailer[2] = 0u;
    trailer[3] = 0u;

    if (fwrite(palette, 1, exported_bytes, stream) != exported_bytes) {
        status = SIXEL_LIBC_ERROR;
        return status;
    }
    if (pad_bytes > 0u &&
            fwrite(zero_pad, 1, pad_bytes, stream) != pad_bytes) {
        status = SIXEL_LIBC_ERROR;
        return status;
    }
    if (fwrite(trailer, 1, sizeof(trailer), stream)
            != sizeof(trailer)) {
        status = SIXEL_LIBC_ERROR;
        return status;
    }

    return SIXEL_OK;
}


SIXELSTATUS
sixel_palette_write_pal_jasc(FILE *stream,
                             unsigned char const *palette,
                             int exported_colors)
{
    int index;

    if (stream == NULL || palette == NULL || exported_colors <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (fprintf(stream, "JASC-PAL\n0100\n%d\n", exported_colors) < 0) {
        return SIXEL_LIBC_ERROR;
    }
    for (index = 0; index < exported_colors; ++index) {
        if (fprintf(stream, "%d %d %d\n",
                    (int)palette[index * 3 + 0],
                    (int)palette[index * 3 + 1],
                    (int)palette[index * 3 + 2]) < 0) {
            return SIXEL_LIBC_ERROR;
        }
    }
    return SIXEL_OK;
}


SIXELSTATUS
sixel_palette_write_pal_riff(FILE *stream,
                             unsigned char const *palette,
                             int exported_colors)
{
    unsigned char size_le[4];
    unsigned char data_size_le[4];
    unsigned char log_palette[4 + 256 * 4];
    unsigned int data_size;
    unsigned int riff_size;
    int index;

    if (stream == NULL || palette == NULL || exported_colors <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (exported_colors > 256) {
        exported_colors = 256;
    }

    data_size = 4u + (unsigned int)exported_colors * 4u;
    riff_size = 4u + 8u + data_size;

    size_le[0] = (unsigned char)(riff_size & 0xffu);
    size_le[1] = (unsigned char)((riff_size >> 8) & 0xffu);
    size_le[2] = (unsigned char)((riff_size >> 16) & 0xffu);
    size_le[3] = (unsigned char)((riff_size >> 24) & 0xffu);
    data_size_le[0] = (unsigned char)(data_size & 0xffu);
    data_size_le[1] = (unsigned char)((data_size >> 8) & 0xffu);
    data_size_le[2] = (unsigned char)((data_size >> 16) & 0xffu);
    data_size_le[3] = (unsigned char)((data_size >> 24) & 0xffu);

    memset(log_palette, 0, sizeof(log_palette));
    log_palette[0] = 0x00;
    log_palette[1] = 0x03;
    log_palette[2] = (unsigned char)(exported_colors & 0xff);
    log_palette[3] = (unsigned char)((exported_colors >> 8) & 0xff);
    for (index = 0; index < exported_colors; ++index) {
        log_palette[4 + index * 4 + 0] = palette[index * 3 + 0];
        log_palette[4 + index * 4 + 1] = palette[index * 3 + 1];
        log_palette[4 + index * 4 + 2] = palette[index * 3 + 2];
        log_palette[4 + index * 4 + 3] = 0u;
    }

    if (fwrite("RIFF", 1, 4u, stream) != 4u) {
        return SIXEL_LIBC_ERROR;
    }
    if (fwrite(size_le, 1, sizeof(size_le), stream) != sizeof(size_le)) {
        return SIXEL_LIBC_ERROR;
    }
    if (fwrite("PAL ", 1, 4u, stream) != 4u) {
        return SIXEL_LIBC_ERROR;
    }
    if (fwrite("data", 1, 4u, stream) != 4u) {
        return SIXEL_LIBC_ERROR;
    }
    if (fwrite(data_size_le, 1, sizeof(data_size_le), stream)
            != sizeof(data_size_le)) {
        return SIXEL_LIBC_ERROR;
    }
    if (fwrite(log_palette, 1, (size_t)data_size, stream)
            != (size_t)data_size) {
        return SIXEL_LIBC_ERROR;
    }
    return SIXEL_OK;
}


SIXELSTATUS
sixel_palette_write_gpl(FILE *stream,
                        unsigned char const *palette,
                        int exported_colors)
{
    int index;

    if (stream == NULL || palette == NULL || exported_colors <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (fprintf(stream, "GIMP Palette\n") < 0) {
        return SIXEL_LIBC_ERROR;
    }
    if (fprintf(stream, "Name: libsixel export\n") < 0) {
        return SIXEL_LIBC_ERROR;
    }
    if (fprintf(stream, "Columns: 16\n") < 0) {
        return SIXEL_LIBC_ERROR;
    }
    if (fprintf(stream, "# Exported by libsixel\n") < 0) {
        return SIXEL_LIBC_ERROR;
    }
    for (index = 0; index < exported_colors; ++index) {
        if (fprintf(stream, "%3d %3d %3d\tIndex %d\n",
                    (int)palette[index * 3 + 0],
                    (int)palette[index * 3 + 1],
                    (int)palette[index * 3 + 2],
                    index) < 0) {
            return SIXEL_LIBC_ERROR;
        }
    }
    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 : */
/* EOF */
