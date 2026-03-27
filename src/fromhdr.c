/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>

#include <sixel.h>

#include "fromhdr.h"

int
stbi_is_hdr_from_memory(unsigned char const *buffer, int len);

float *
stbi_loadf_from_memory(unsigned char const *buffer,
                       int len,
                       int *x,
                       int *y,
                       int *channels_in_file,
                       int desired_channels);

char const *
stbi_failure_reason(void);

void
stbi_image_free(void *retval_from_stbi_load);

SIXELSTATUS
sixel_builtin_decode_hdr_float32(
    sixel_chunk_t const *chunk,
    unsigned char **ppixels,
    int *pwidth,
    int *pheight,
    int *ppixelformat,
    int *pcolorspace)
{
    float *decoded_pixels;
    int depth;
    char const *reason;

    decoded_pixels = NULL;
    depth = 0;
    reason = NULL;

    if (chunk == NULL ||
        chunk->buffer == NULL ||
        ppixels == NULL ||
        pwidth == NULL ||
        pheight == NULL ||
        ppixelformat == NULL ||
        pcolorspace == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppixels = NULL;
    *pwidth = 0;
    *pheight = 0;
    *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
    *pcolorspace = SIXEL_COLORSPACE_GAMMA;

    if (chunk->size == 0u) {
        return SIXEL_FALSE;
    }
    if (chunk->size > (size_t)INT_MAX) {
        sixel_helper_set_additional_message(
            "builtin HDR: input chunk is too large.");
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if (!stbi_is_hdr_from_memory(chunk->buffer, (int)chunk->size)) {
        return SIXEL_FALSE;
    }

    decoded_pixels = stbi_loadf_from_memory(chunk->buffer,
                                            (int)chunk->size,
                                            pwidth,
                                            pheight,
                                            &depth,
                                            3);
    if (decoded_pixels == NULL) {
        reason = stbi_failure_reason();
        if (reason != NULL) {
            sixel_helper_set_additional_message(reason);
        }
        return SIXEL_STBI_ERROR;
    }
    if (*pwidth <= 0 || *pheight <= 0) {
        stbi_image_free(decoded_pixels);
        sixel_helper_set_additional_message(
            "builtin HDR: invalid image dimensions.");
        return SIXEL_STBI_ERROR;
    }

    *ppixels = (unsigned char *)decoded_pixels;
    *ppixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    *pcolorspace = SIXEL_COLORSPACE_LINEAR;

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
