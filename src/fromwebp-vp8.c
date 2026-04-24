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

#include <stddef.h>

#include "compat_stub.h"
#include "fromwebp-vp8-private.h"

SIXELSTATUS
sixel_webp_decode_vp8_payload(unsigned char const *payload,
                              size_t payload_size,
                              unsigned char **prgba,
                              int *pwidth,
                              int *pheight,
                              sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_webp_vp8_frame_header_t header;
#if HAVE_WEBP
    unsigned char *riff_data;
    size_t riff_size;
#endif

    status = SIXEL_OK;
    header.width = 0;
    header.height = 0;
    header.version = 0;
    header.show_frame = 0;
    header.first_partition_size = 0u;
#if HAVE_WEBP
    riff_data = NULL;
    riff_size = 0u;
#endif
    if (payload == NULL || payload_size == 0u || prgba == NULL ||
        pwidth == NULL || pheight == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *prgba = NULL;
    *pwidth = 0;
    *pheight = 0;

    status = sixel_webp_vp8_parse_header(payload, payload_size, &header);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    /*
     * Native decode is now the first-class path. While the implementation is
     * still being completed, keep libwebp as a compatibility fallback only
     * when the native path reports NOT_IMPLEMENTED.
     */
    status = sixel_webp_vp8_decode_native_payload(payload,
                                                  payload_size,
                                                  &header,
                                                  prgba,
                                                  pwidth,
                                                  pheight,
                                                  allocator);
    if (SIXEL_SUCCEEDED(status) || status != SIXEL_NOT_IMPLEMENTED) {
        return status;
    }

#if HAVE_WEBP
    status = sixel_webp_vp8_wrap_riff_payload(payload,
                                              payload_size,
                                              &riff_data,
                                              &riff_size,
                                              allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_webp_vp8_decode_with_libwebp(riff_data,
                                                riff_size,
                                                &header,
                                                prgba,
                                                pwidth,
                                                pheight,
                                                allocator);
    sixel_allocator_free(allocator, riff_data);
#else
    sixel_helper_set_additional_message(
        "builtin webp: VP8 static decoder is unavailable in this build.");
#endif
    return status;
}


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
