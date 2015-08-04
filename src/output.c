/*
 * Copyright (c) 2014,2015 Hayaki Saito
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
#include <assert.h>
#include <sixel.h>
#include "output.h"


SIXELAPI SIXELSTATUS
sixel_output_new(
    sixel_output_t          /* out */ **output,
    sixel_write_function    /* in */  fn_write,
    void                    /* in */  *priv,
    sixel_allocator_t       /* in */  *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t size;

    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        sixel_allocator_ref(allocator);
    }
    size = sizeof(sixel_output_t) + SIXEL_OUTPUT_PACKET_SIZE * 2;

    *output = (sixel_output_t *)sixel_allocator_malloc(allocator, size);
    if (*output == NULL) {
        sixel_helper_set_additional_message(
            "sixel_output_new: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    (*output)->ref = 1;
    (*output)->has_8bit_control = 0;
    (*output)->has_sdm_glitch = 0;
    (*output)->skip_dcs_envelope = 0;
    (*output)->palette_type = SIXEL_PALETTETYPE_AUTO;
    (*output)->fn_write = fn_write;
    (*output)->save_pixel = 0;
    (*output)->save_count = 0;
    (*output)->active_palette = (-1);
    (*output)->node_top = NULL;
    (*output)->node_free = NULL;
    (*output)->priv = priv;
    (*output)->pos = 0;
    (*output)->penetrate_multiplexer = 0;
    (*output)->encode_policy = SIXEL_ENCODEPOLICY_AUTO;
    (*output)->allocator = allocator;

    status = SIXEL_OK;

end:
    return status;
}


SIXELAPI sixel_output_t *
sixel_output_create(sixel_write_function fn_write, void *priv)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_output_t *output = NULL;

    status = sixel_output_new(&output, fn_write, priv, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    return output;
}

 
SIXELAPI void
sixel_output_destroy(sixel_output_t *output)
{
    sixel_allocator_t *allocator;

    if (output) {
        allocator = output->allocator;
        sixel_allocator_free(allocator, output);
        sixel_allocator_unref(allocator);
    }
}


SIXELAPI void
sixel_output_ref(sixel_output_t *output)
{
    /* TODO: be thread-safe */
    ++output->ref;
}


SIXELAPI void
sixel_output_unref(sixel_output_t *output)
{
    /* TODO: be thread-safe */
    if (output) {
        assert(output->ref > 0);
        output->ref--;
        if (output->ref == 0) {
            sixel_output_destroy(output);
        }
    }
}


SIXELAPI int
sixel_output_get_8bit_availability(sixel_output_t *output)
{
    return output->has_8bit_control;
}


SIXELAPI void
sixel_output_set_8bit_availability(sixel_output_t *output, int availability)
{
    output->has_8bit_control = availability;
}


SIXELAPI void
sixel_output_set_penetrate_multiplexer(sixel_output_t *output, int penetrate)
{
    output->penetrate_multiplexer = penetrate;
}


SIXELAPI void
sixel_output_set_skip_dcs_envelope(sixel_output_t *output, int skip)
{
    output->skip_dcs_envelope = skip;
}


SIXELAPI void
sixel_output_set_palette_type(sixel_output_t *output, int palettetype)
{
    output->palette_type = palettetype;
}


SIXELAPI void
sixel_output_set_encode_policy(sixel_output_t *output, int encode_policy)
{
    output->encode_policy = encode_policy;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
