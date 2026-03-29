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

#ifndef LIBSIXEL_MAPFILE_H
#define LIBSIXEL_MAPFILE_H

#include <stddef.h>
#include <stdio.h>

#include <sixel.h>

typedef enum sixel_palette_format {
    SIXEL_PALETTE_FORMAT_NONE = 0,
    SIXEL_PALETTE_FORMAT_ACT,
    SIXEL_PALETTE_FORMAT_PAL_JASC,
    SIXEL_PALETTE_FORMAT_PAL_RIFF,
    SIXEL_PALETTE_FORMAT_PAL_AUTO,
    SIXEL_PALETTE_FORMAT_GPL
} sixel_palette_format_t;

char const *
sixel_palette_strip_prefix(char const *spec,
                           sixel_palette_format_t *format_hint);

sixel_palette_format_t
sixel_palette_format_from_extension(char const *path);

int
sixel_path_has_any_extension(char const *path);

SIXELSTATUS
sixel_palette_read_stream(FILE *stream,
                          sixel_allocator_t *allocator,
                          unsigned char **pdata,
                          size_t *psize);

SIXELSTATUS
sixel_palette_open_read(char const *path, FILE **pstream, int *pclose);

void
sixel_palette_close_stream(FILE *stream, int close_stream);

sixel_palette_format_t
sixel_palette_guess_format(unsigned char const *data, size_t size);

SIXELSTATUS
sixel_palette_parse_act(unsigned char const *data,
                        size_t size,
                        sixel_encoder_t *encoder,
                        sixel_dither_t **dither);

SIXELSTATUS
sixel_palette_parse_pal_jasc(unsigned char const *data,
                             size_t size,
                             sixel_encoder_t *encoder,
                             sixel_dither_t **dither);

SIXELSTATUS
sixel_palette_parse_pal_riff(unsigned char const *data,
                             size_t size,
                             sixel_encoder_t *encoder,
                             sixel_dither_t **dither);

SIXELSTATUS
sixel_palette_parse_gpl(unsigned char const *data,
                        size_t size,
                        sixel_encoder_t *encoder,
                        sixel_dither_t **dither);

SIXELSTATUS
sixel_palette_write_act(FILE *stream,
                        unsigned char const *palette,
                        size_t palette_bytes,
                        int exported_colors);

SIXELSTATUS
sixel_palette_write_pal_jasc(FILE *stream,
                             unsigned char const *palette,
                             int exported_colors);

SIXELSTATUS
sixel_palette_write_pal_riff(FILE *stream,
                             unsigned char const *palette,
                             int exported_colors);

SIXELSTATUS
sixel_palette_write_gpl(FILE *stream,
                        unsigned char const *palette,
                        int exported_colors);

#endif /* LIBSIXEL_MAPFILE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
