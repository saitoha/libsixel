/*
 * SPDX-License-Identifier: MIT AND BSD-3-Clause
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 * Copyright (c) Araki Ken(arakiken@users.sourceforge.net)
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
 *
 * --------------------------------------------------------------------------
 * Portions of this file(sixel_drcs_emit_drcsmmv2_chars) are derived from
 * mlterm's drcssixel.c.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of any author may not be used to endorse or promote
 *    products derived from this software without their specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_ERRNO_H
#include <errno.h>
#endif  /* HAVE_ERRNO_H */
#if HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */

#include <sixel.h>

#include "compat_stub.h"
#include "drcs.h"
#include "encoder.h"

static int
sixel_drcs_probe_fd_write(int fd, char *data, int size)
{
    return (int)sixel_compat_write(fd, data, (size_t)size);
}


static void
sixel_drcs_fill_designation(sixel_encoder_t *encoder,
                            int *drcs_is_96cs_param,
                            char drcs_designate_str[4])
{
    int drcs_is_96cs;
    unsigned int charset_no;

    drcs_is_96cs = 0;
    charset_no = 1u;

    drcs_designate_str[0] = 0x20;
    drcs_designate_str[1] = 0x20;
    drcs_designate_str[2] = 0x40;
    drcs_designate_str[3] = 0x00;

    charset_no = encoder->drcs_charset_no;
    if (charset_no == 0u) {
        charset_no = 1u;
    }

    if (encoder->drcs_mmv == 0) {
        drcs_is_96cs = (charset_no > 63u) ? 1 : 0;
        drcs_designate_str[1] =
            (char)(((charset_no - 1u) % 63u) + 0x40u);
        drcs_designate_str[2] = 0x00;
    } else if (encoder->drcs_mmv == 1) {
        drcs_is_96cs = 0;
        drcs_designate_str[1] = (char)(charset_no + 0x3fu);
        drcs_designate_str[2] = 0x00;
    } else if (encoder->drcs_mmv == 2) {
        drcs_is_96cs = (charset_no > 79u) ? 1 : 0;
        drcs_designate_str[1] =
            (char)(((charset_no - 1u) % 79u) + 0x30u);
        drcs_designate_str[2] = 0x00;
    } else {
        drcs_is_96cs = 0;
        drcs_designate_str[1] =
            (char)(((charset_no - 1u) / 63u) + 0x20u);
        drcs_designate_str[2] =
            (char)(((charset_no - 1u) % 63u) + 0x40u);
        drcs_designate_str[3] = 0x00;
    }

    *drcs_is_96cs_param = drcs_is_96cs;
}


static SIXELSTATUS
sixel_drcs_emit_iso2022_chars(sixel_encoder_t *encoder,
                              sixel_frame_t *frame)
{
    char *buf_p;
    char *buf;
    int col;
    int row;
    int charset;
    int is_96cs;
    unsigned int charset_no;
    unsigned int code;
    int num_cols;
    int num_rows;
    SIXELSTATUS status;
    size_t alloc_size;
    int nwrite;
    int target_fd;
    int chunk_size;

    charset_no = encoder->drcs_charset_no;
    if (charset_no == 0u) {
        charset_no = 1u;
    }
    if (encoder->drcs_mmv == 0) {
        is_96cs = (charset_no > 63u) ? 1 : 0;
        charset = (int)(((charset_no - 1u) % 63u) + 0x40u);
    } else if (encoder->drcs_mmv == 1) {
        is_96cs = 0;
        charset = (int)(charset_no + 0x3fu);
    } else {
        is_96cs = (charset_no > 79u) ? 1 : 0;
        charset = (int)(((charset_no - 1u) % 79u) + 0x30u);
    }
    code = 0x100020 + (is_96cs ? 0x80 : 0) + charset * 0x100;
    num_cols = (sixel_frame_get_width(frame) + encoder->cell_width - 1)
             / encoder->cell_width;
    num_rows = (sixel_frame_get_height(frame) + encoder->cell_height - 1)
             / encoder->cell_height;

    /* cols x rows + designation(4 chars) + SI + SO + LFs */
    alloc_size = num_cols * num_rows + (num_cols * num_rows / 96 + 1) * 4
               + 2 + num_rows;
    buf_p = buf = sixel_allocator_malloc(encoder->allocator, alloc_size);
    if (buf == NULL) {
        sixel_helper_set_additional_message(
            "sixel_drcs_emit_iso2022_chars: "
            "sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    code = 0x20;
    *(buf_p++) = '\016';  /* SI */
    *(buf_p++) = '\033';
    *(buf_p++) = ')';
    *(buf_p++) = ' ';
    *(buf_p++) = (char)charset;
    for (row = 0; row < num_rows; row++) {
        for (col = 0; col < num_cols; col++) {
            if ((code & 0x7f) == 0x0) {
                if (charset == 0x7e) {
                    is_96cs = 1 - is_96cs;
                    charset = '0';
                } else {
                    charset++;
                }
                code = 0x20;
                *(buf_p++) = '\033';
                *(buf_p++) = is_96cs ? '-' : ')';
                *(buf_p++) = ' ';
                *(buf_p++) = (char)charset;
            }
            *(buf_p++) = (char)code++;
        }
        *(buf_p++) = '\n';
    }
    *(buf_p++) = '\017';  /* SO */

    if (encoder->tile_outfd >= 0) {
        target_fd = encoder->tile_outfd;
    } else {
        target_fd = encoder->outfd;
    }

    chunk_size = (int)(buf_p - buf);
    nwrite = sixel_drcs_probe_fd_write(target_fd, buf, chunk_size);
    if (nwrite != chunk_size) {
        sixel_helper_set_additional_message(
            "sixel_drcs_emit_iso2022_chars: write() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    sixel_allocator_free(encoder->allocator, buf);

    status = SIXEL_OK;

end:
    return status;
}


/*
 * This routine is derived from mlterm's drcssixel.c
 * (https://raw.githubusercontent.com/arakiken/mlterm/master/drcssixel/
 * drcssixel.c).
 * The original implementation is credited to Araki Ken.
 * Adjusted here to integrate with libsixel's encoder pipeline.
 */
static SIXELSTATUS
sixel_drcs_emit_drcsmmv2_chars(sixel_encoder_t *encoder,
                               sixel_frame_t *frame)
{
    char *buf_p;
    char *buf;
    int col;
    int row;
    char ibytes[3] = { 0x20, 0x00, 0x00 };
    int is_96cs;
    unsigned int charset_no;
    unsigned int code;
    int num_cols;
    int num_rows;
    SIXELSTATUS status;
    size_t alloc_size;
    int nwrite;
    int target_fd;
    int chunk_size;
    int fill;

    charset_no = encoder->drcs_charset_no;
    if (charset_no == 0u) {
        charset_no = 1u;
    }
    if (encoder->drcs_mmv == 0) {
        is_96cs = (charset_no > 63u) ? 1 : 0;
        ibytes[1] = (char)(((charset_no - 1u) % 63u) + 0x40u);
        fill = 0;
    } else if (encoder->drcs_mmv == 1) {
        is_96cs = 0;
        ibytes[1] = (char)(charset_no + 0x3fu);
        fill = 0;
    } else if (encoder->drcs_mmv == 2) {
        is_96cs = (charset_no > 79u) ? 1 : 0;
        ibytes[1] = (char)(((charset_no - 1u) % 79u) + 0x30u);
        fill = 0;
    } else {  /* v3 */
        is_96cs = 0;
        ibytes[1] = (char)(((charset_no - 1u) / 63u) + 0x20u);
        ibytes[2] = (char)(((charset_no - 1u) % 63u) + 0x40u);
        fill = 1;
    }
    if (fill) {
        code = 0x100000 + (charset_no - 1u) * 94;
    } else {
        code = 0x100020 + (is_96cs ? 0x80 : 0) + ibytes[1] * 0x100;
    }
    num_cols = (sixel_frame_get_width(frame) + encoder->cell_width - 1)
             / encoder->cell_width;
    num_rows = (sixel_frame_get_height(frame) + encoder->cell_height - 1)
             / encoder->cell_height;

    /* cols x rows x 4(out of BMP) + rows(LFs) */
    alloc_size = num_cols * num_rows * 4 + num_rows;
    buf_p = buf = sixel_allocator_malloc(encoder->allocator, alloc_size);
    if (buf == NULL) {
        sixel_helper_set_additional_message(
            "sixel_drcs_emit_drcsmmv2_chars: "
            "sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (row = 0; row < num_rows; row++) {
        for (col = 0; col < num_cols; col++) {
            *(buf_p++) = ((code >> 18) & 0x07) | 0xf0;
            *(buf_p++) = ((code >> 12) & 0x3f) | 0x80;
            *(buf_p++) = ((code >> 6) & 0x3f) | 0x80;
            *(buf_p++) = (code & 0x3f) | 0x80;
            code++;
            if (!fill) {
                if ((code & 0x7f) == 0x0) {
                    if (ibytes[1] == 0x7e) {
                        is_96cs = 1 - is_96cs;
                        ibytes[1] = '0';
                    } else {
                        ibytes[1]++;
                    }
                    code = 0x100020 + (is_96cs ? 0x80 : 0)
                         + ibytes[1] * 0x100;
                }
            }
        }
        *(buf_p++) = '\n';
    }

    if (encoder->tile_outfd >= 0) {
        target_fd = encoder->tile_outfd;
    } else {
        target_fd = encoder->outfd;
    }

    chunk_size = (int)(buf_p - buf);
    nwrite = sixel_drcs_probe_fd_write(target_fd, buf, chunk_size);
    if (nwrite != chunk_size) {
        sixel_helper_set_additional_message(
            "sixel_drcs_emit_drcsmmv2_chars: write() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    sixel_allocator_free(encoder->allocator, buf);

    status = SIXEL_OK;

end:
    return status;
}


SIXELSTATUS
sixel_drcs_emit_begin_sequence(sixel_encoder_t *encoder,
                               sixel_write_function write_callback,
                               void *write_priv)
{
    SIXELSTATUS status;
    char buf[256];
    int nwrite;
    int drcs_is_96cs_param;
    char drcs_designate_str[4];

    status = SIXEL_FALSE;
    nwrite = 0;
    drcs_is_96cs_param = 0;
    drcs_designate_str[0] = 0x20;
    drcs_designate_str[1] = 0x20;
    drcs_designate_str[2] = 0x40;
    drcs_designate_str[3] = 0x00;

    if (encoder == NULL || write_callback == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_drcs_fill_designation(encoder,
                                &drcs_is_96cs_param,
                                drcs_designate_str);

    nwrite = sixel_compat_snprintf(
        buf,
        sizeof(buf),
        "%s%sh%s1;0;0;%d;1;3;%d;%d{%s",
        (encoder->drcs_mmv > 0)
            ? (encoder->f8bit ? "\233?8800" : "\033[?8800")
            : "",
        (encoder->drcs_mmv >= 3)
            ? (encoder->f8bit ? ";8801" : ";8801")
            : "",
        encoder->f8bit ? "\220" : "\033P",
        encoder->cell_width,
        encoder->cell_height,
        drcs_is_96cs_param,
        drcs_designate_str);
    if (nwrite < 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message(
            "sixel_drcs_emit_begin_sequence: command format failed.");
        return status;
    }

    nwrite = write_callback(buf, nwrite, write_priv);
    if (nwrite < 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message(
            "sixel_drcs_emit_begin_sequence: write() failed.");
        return status;
    }

    return SIXEL_OK;
}


SIXELSTATUS
sixel_drcs_emit_tile_chars(sixel_encoder_t *encoder,
                           sixel_frame_t *frame)
{
    if (encoder == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (encoder->drcs_mmv == 0) {
        return sixel_drcs_emit_iso2022_chars(encoder, frame);
    }

    return sixel_drcs_emit_drcsmmv2_chars(encoder, frame);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 : */
/* EOF */
