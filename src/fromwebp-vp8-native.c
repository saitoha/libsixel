/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See AUTHORS.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

/* STDC_HEADERS */
#include <stddef.h>

#include "compat_stub.h"
#include "fromwebp-vp8-private.h"

SIXELSTATUS
sixel_webp_vp8_decode_native_payload(unsigned char const *payload,
                                     size_t payload_size,
                                     sixel_webp_vp8_frame_header_t const
                                         *header,
                                     unsigned char **prgba,
                                     int *pwidth,
                                     int *pheight,
                                     sixel_allocator_t *allocator)
{
    /*
     * The native VP8 pipeline is intentionally split in this translation
     * unit so upcoming stages can add:
     *  - bool/range reader and partition parser
     *  - macroblock mode and coefficient decode
     *  - inverse transform, loop filter, and YUV-to-RGBA conversion
     * while keeping the public entrypoint in fromwebp-vp8.c minimal.
     */
    if (payload == NULL || payload_size == 0u || header == NULL ||
        prgba == NULL || pwidth == NULL || pheight == NULL ||
        allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *prgba = NULL;
    *pwidth = 0;
    *pheight = 0;
    (void)header;
    (void)allocator;

    sixel_helper_set_additional_message(
        "builtin webp: native VP8 static decoder is not ready yet.");
    return SIXEL_NOT_IMPLEMENTED;
}


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
