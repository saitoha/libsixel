/*
 * Copyright (c) 2014 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_OUTPUT_H
#define LIBSIXEL_OUTPUT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sixel_node {
    struct sixel_node *next;
    int pal;
    int sx;
    int mx;
    unsigned char *map;
} sixel_node_t;

typedef int (* sixel_write_function)(char *data, int size, void *priv);

typedef struct sixel_output {

    int ref;

    /* compatiblity flags */

    /* 0: 7bit terminal,
     * 1: 8bit terminal */
    unsigned char has_8bit_control;

    /* 0: the terminal has sixel scrolling
     * 1: the terminal does not have sixel scrolling */
    unsigned char has_sixel_scrolling;

    /* 0: DECSDM set (CSI ? 80 h) enables sixel scrolling
       1: DECSDM set (CSI ? 80 h) disables sixel scrolling */
    unsigned char has_sdm_glitch;

    sixel_write_function fn_write;

    unsigned char conv_palette[256];
    int save_pixel;
    int save_count;
    int active_palette;

    sixel_node_t *node_top;
    sixel_node_t *node_free;

    void *priv;
    int pos;
    unsigned char buffer[1];

} sixel_output_t;

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_OUTPUT_H */

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
