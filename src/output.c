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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include "output.h"
#include "sixel.h"


sixel_output_t * const
sixel_output_create(sixel_write_function fn_write, void *priv)
{
    sixel_output_t *output;
   
    output = malloc(sizeof(sixel_output_t) + SIXEL_OUTPUT_PACKET_SIZE * 2);
    output->ref = 1;
    output->has_8bit_control = 0;
    output->has_sdm_glitch = 0;
    output->fn_write = fn_write;
    output->save_pixel = 0;
    output->save_count = 0;
    output->active_palette = (-1);
    output->node_top = NULL;
    output->node_free = NULL;
    output->priv = priv;
    output->pos = 0;

    return output;
}


void
sixel_output_destroy(sixel_output_t *output)
{
    free(output);
}


void
sixel_output_ref(sixel_output_t *output)
{
    /* TODO: be thread-safe */
    ++output->ref;
}

void
sixel_output_unref(sixel_output_t *output)
{
    /* TODO: be thread-safe */
    if (output && --output->ref == 0) {
        sixel_output_destroy(output);
    }
}

int
sixel_output_get_8bit_availability(sixel_output_t *output)
{
    return output->has_8bit_control;
}


void
sixel_output_set_8bit_availability(sixel_output_t *output, int availability)
{
    output->has_8bit_control = availability;
}


/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
