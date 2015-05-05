/*
 * Copyright (c) 2015 Hayaki Saito
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "frame.h"
#include <sixel.h>


typedef unsigned char stbi_uc;

#ifdef _MSC_VER
typedef unsigned short stbi__uint16;
typedef   signed short stbi__int16;
typedef unsigned int   stbi__uint32;
typedef   signed int   stbi__int32;
#else
#include <stdint.h>
typedef uint16_t stbi__uint16;
typedef int16_t  stbi__int16;
typedef uint32_t stbi__uint32;
typedef int32_t  stbi__int32;
#endif

///////////////////////////////////////////////
//
//  stbi__context struct and start_xxx functions

// stbi__context structure is our basic context used by all images, so it
// contains all the IO context, plus some basic image information
typedef struct
{
   stbi__uint32 img_x, img_y;
   int img_n, img_out_n;

   int buflen;
   stbi_uc buffer_start[128];

   stbi_uc *img_buffer, *img_buffer_end;
   stbi_uc *img_buffer_original;
} stbi__context;

typedef struct
{
   stbi__int16 prefix;
   stbi_uc first;
   stbi_uc suffix;
} stbi__gif_lzw;

typedef struct
{
   int w,h;
   stbi_uc *out;                 // output buffer (always 4 components)
   int flags, bgindex, ratio, transparent, eflags;
   stbi_uc  pal[256][3];
   stbi_uc lpal[256][3];
   stbi__gif_lzw codes[4096];
   stbi_uc *color_table;
   int parse, step;
   int lflags;
   int start_x, start_y;
   int max_x, max_y;
   int cur_x, cur_y;
   int line_size;
   int loop_count;
   int delay;
   int is_multiframe;
   int is_terminated;
} stbi__gif;



// initialize a memory-decode context
static void
start_mem(stbi__context *s, stbi_uc const *buffer, int len)
{
    s->img_buffer = s->img_buffer_original = (stbi_uc *) buffer;
    s->img_buffer_end = (stbi_uc *) buffer+len;
}


static
stbi_uc get8(stbi__context *s)
{
    if (s->img_buffer < s->img_buffer_end) {
        return *s->img_buffer++;
    }
    return 0;
}


static int get16le(stbi__context *s)
{
    int z = get8(s);
    return z + (get8(s) << 8);
}


static int
stbi__getn(stbi__context *s, stbi_uc *buffer, int n)
{
   if (s->img_buffer+n <= s->img_buffer_end) {
      memcpy(buffer, s->img_buffer, n);
      s->img_buffer += n;
      return 1;
   } else
      return 0;
}


static void
stbi__skip(stbi__context *s, int n)
{
    s->img_buffer += n;
}


static void
stbi__rewind(stbi__context *s)
{
    s->img_buffer = s->img_buffer_original;
}


static void
gif_parse_colortable(
    stbi__context *s,
    stbi_uc pal[256][3],
    int num_entries)
{
    int i;

    for (i = 0; i < num_entries; ++i) {
        pal[i][2] = get8(s);
        pal[i][1] = get8(s);
        pal[i][0] = get8(s);
    }
}


static int
load_gif_header(stbi__context *s, stbi__gif *g)
{
    stbi_uc version;
    if (get8(s) != 'G') {
        return (-1);
    }
    if (get8(s) != 'I') {
        return (-1);
    }
    if (get8(s) != 'F') {
        return (-1);
    }
    if (get8(s) != '8') {
        return (-1);
    }

    version = get8(s);

    if (version != '7' && version != '9') {
        return (-1);
    }
    if (get8(s) != 'a') {
        return (-1);
    }

    g->w = get16le(s);
    g->h = get16le(s);
    g->flags = get8(s);
    g->bgindex = get8(s);
    g->ratio = get8(s);
    g->transparent = -1;
    g->loop_count = -1;

    if (g->flags & 0x80) {
        gif_parse_colortable(s,g->pal, 2 << (g->flags & 7));
    }

    return 0;
}


int
init_gif_frame(
    sixel_frame_t *frame,
    stbi__gif *pg,
    unsigned char *bgcolor,
    int reqcolors,
    int fuse_palette)
{
    int i;

    frame->delay = pg->delay;
    frame->ncolors = 2 << (pg->flags & 7);
    frame->palette = malloc(frame->ncolors * 3);
    if (frame->palette == NULL) {
        return (-1);
    }
    if (frame->ncolors <= reqcolors && fuse_palette) {
        frame->pixelformat = PIXELFORMAT_PAL8;
        frame->pixels = malloc(frame->width * frame->height);
        memcpy(frame->pixels, pg->out, frame->width * frame->height);

        for (i = 0; i < frame->ncolors; ++i) {
            frame->palette[i * 3 + 0] = pg->color_table[i * 3 + 2];
            frame->palette[i * 3 + 1] = pg->color_table[i * 3 + 1];
            frame->palette[i * 3 + 2] = pg->color_table[i * 3 + 0];
        }
        if (pg->lflags & 0x80) {
            if (pg->eflags & 0x01) {
                if (bgcolor) {
                    frame->palette[pg->transparent * 3 + 0] = bgcolor[0];
                    frame->palette[pg->transparent * 3 + 1] = bgcolor[1];
                    frame->palette[pg->transparent * 3 + 2] = bgcolor[2];
                } else {
                    frame->transparent = pg->transparent;
                }
            }
        } else if (pg->flags & 0x80) {
            if (pg->eflags & 0x01) {
                if (bgcolor) {
                    frame->palette[pg->transparent * 3 + 0] = bgcolor[0];
                    frame->palette[pg->transparent * 3 + 1] = bgcolor[1];
                    frame->palette[pg->transparent * 3 + 2] = bgcolor[2];
                } else {
                    frame->transparent = pg->transparent;
                }
            }
        }
    } else {
        frame->pixelformat = PIXELFORMAT_RGB888;
        frame->pixels = malloc(pg->w * pg->h * 3);
        for (i = 0; i < pg->w * pg->h; ++i) {
            frame->pixels[i * 3 + 0] = pg->color_table[pg->out[i] * 3 + 2];
            frame->pixels[i * 3 + 1] = pg->color_table[pg->out[i] * 3 + 1];
            frame->pixels[i * 3 + 2] = pg->color_table[pg->out[i] * 3 + 0];
        }
    }
    if (frame->pixels == NULL) {
        fprintf(stderr, "stbi_load_from_file failed.\n");
        return (-1);
    }
    frame->multiframe = (pg->loop_count != (-1));

    return 0;
}


static void
out_gif_code(
    stbi__gif *g,
    stbi__uint16 code
)
{
    /* recurse to decode the prefixes, since the linked-list is backwards,
       and working backwards through an interleaved image would be nasty */
    if (g->codes[code].prefix >= 0) {
        out_gif_code(g, g->codes[code].prefix);
    }

    if (g->cur_y >= g->max_y) {
        return;
    }

    g->out[g->cur_x + g->cur_y] = g->codes[code].suffix;
    g->cur_x++;

    if (g->cur_x >= g->max_x) {
        g->cur_x = g->start_x;
        g->cur_y += g->step;

        while (g->cur_y >= g->max_y && g->parse > 0) {
            g->step = (1 << g->parse) * g->line_size;
            g->cur_y = g->start_y + (g->step >> 1);
            --g->parse;
        }
    }
}


static int
process_gif_raster(
    stbi__context *s,
    stbi__gif *g
)
{
    stbi_uc lzw_cs;
    stbi__int32 len, code;
    stbi__uint32 first;
    stbi__int32 codesize, codemask, avail, oldcode, bits, valid_bits, clear;
    stbi__gif_lzw *p;

    lzw_cs = get8(s);
    clear = 1 << lzw_cs;
    first = 1;
    codesize = lzw_cs + 1;
    codemask = (1 << codesize) - 1;
    bits = 0;
    valid_bits = 0;
    for (code = 0; code < clear; code++) {
        g->codes[code].prefix = -1;
        g->codes[code].first = (stbi_uc) code;
        g->codes[code].suffix = (stbi_uc) code;
    }

    // support no starting clear code
    avail = clear+2;
    oldcode = -1;

    len = 0;
    for(;;) {
        if (valid_bits < codesize) {
            if (len == 0) {
                len = get8(s); // start new block
                if (len == 0) {
                    return SIXEL_SUCCESS;
                }
            }
            --len;
            bits |= (stbi__int32) get8(s) << valid_bits;
            valid_bits += 8;
        } else {
            stbi__int32 code = bits & codemask;
            bits >>= codesize;
            valid_bits -= codesize;
            // @OPTIMIZE: is there some way we can accelerate the non-clear path?
            if (code == clear) {  // clear code
                codesize = lzw_cs + 1;
                codemask = (1 << codesize) - 1;
                avail = clear + 2;
                oldcode = -1;
                first = 0;
            } else if (code == clear + 1) { // end of stream code
                stbi__skip(s, len);
                while ((len = get8(s)) > 0) {
                   stbi__skip(s,len);
                }
                return SIXEL_SUCCESS;
            } else if (code <= avail) {
                if (first) {
                    fprintf(stderr,
                            "Corrupt GIF\n" "reason: no clear code\n");
                    return SIXEL_FAILED;
                }
                if (oldcode >= 0) {
                    p = &g->codes[avail++];
                    if (avail > 4096) {
                        fprintf(stderr,
                                "Corrupt GIF\n" "reason: too many codes\n");
                        return SIXEL_FAILED;
                    }
                    p->prefix = (stbi__int16) oldcode;
                    p->first = g->codes[oldcode].first;
                    p->suffix = (code == avail) ? p->first : g->codes[code].first;
                } else if (code == avail) {
                    fprintf(stderr,
                            "Corrupt GIF\n" "reason: illegal code in raster\n");
                    return SIXEL_FAILED;
                }

                out_gif_code(g, (stbi__uint16) code);

                if ((avail & codemask) == 0 && avail <= 0x0FFF) {
                    codesize++;
                    codemask = (1 << codesize) - 1;
                }

                oldcode = code;
            } else {
                fprintf(stderr,
                        "Corrupt GIF\n" "reason: illegal code in raster\n");
                return SIXEL_FAILED;
            }
        }
    }
}


/* this function is ported from stb_image.h */
static int
gif_load_next(
    stbi__context *s,
    stbi__gif *g,
    stbi_uc *bgcolor
)
{
    stbi_uc buffer[256];

    for (;;) {
        switch (get8(s)) {
        case 0x2C: /* Image Descriptor */
        {
            stbi__int32 x, y, w, h;

            x = get16le(s);
            y = get16le(s);
            w = get16le(s);
            h = get16le(s);
            if (((x + w) > (g->w)) || ((y + h) > (g->h))) {
                fprintf(stderr,
                        "Corrupt GIF.\n" "reason: bad Image Descriptor.\n");
                return SIXEL_FAILED;
            }

            g->line_size = g->w;
            g->start_x = x;
            g->start_y = y * g->line_size;
            g->max_x   = g->start_x + w;
            g->max_y   = g->start_y + h * g->line_size;
            g->cur_x   = g->start_x;
            g->cur_y   = g->start_y;

            g->lflags = get8(s);

            if (g->lflags & 0x40) {
                g->step = 8 * g->line_size; /* first interlaced spacing */
                g->parse = 3;
            } else {
                g->step = g->line_size;
                g->parse = 0;
            }

            if (g->lflags & 0x80) {
                gif_parse_colortable(s,
                                     g->lpal,
                                     2 << (g->lflags & 7));
                g->color_table = (stbi_uc *) g->lpal;
            } else if (g->flags & 0x80) {
                /* @OPTIMIZE: stbi__jpeg_reset only the previous transparent */
                if (g->transparent >= 0 && (g->eflags & 0x01)) {
                   if (bgcolor) {
                       g->pal[g->transparent][0] = bgcolor[2];
                       g->pal[g->transparent][1] = bgcolor[1];
                       g->pal[g->transparent][2] = bgcolor[0];
                   }
                }
                g->color_table = (stbi_uc *)g->pal;
            } else {
                fprintf(stderr,
                        "Corrupt GIF.\n" "reason: missing color table.\n");
                return SIXEL_FAILED;
            }

            return process_gif_raster(s, g);
        }

        case 0x21: // Comment Extension.
        {
            int len;
            switch (get8(s)) {
            case 0x01: // Plain Text Extension
                break;
            case 0x21: // Comment Extension
                break;
            case 0xF9: // Graphic Control Extension
                len = get8(s); // block size
                if (len == 4) {
                    g->eflags = get8(s);
                    g->delay = get16le(s); // delay
                    g->transparent = get8(s);
                } else {
                    stbi__skip(s, len);
                    break;
                }
                break;
            case 0xFF: // Application Extension
                len = get8(s); // block size
                stbi__getn(s, buffer, len);
                buffer[len] = 0;
                if (len == 11 && strcmp((char *)buffer, "NETSCAPE2.0") == 0) {
                    if (get8(s) == 0x03) {
                        /* loop count */
                        switch (get8(s)) {
                        case 0x00:
                            g->loop_count = 1;
                            break;
                        case 0x01:
                            g->loop_count = get16le(s);
                            break;
                        }
                    }
                }
                break;
            default:
                break;
            }
            while ((len = get8(s)) != 0) {
                stbi__skip(s, len);
            }
            break;
        }

        case 0x3B: // gif stream termination code
            g->is_terminated = 1;
            return SIXEL_SUCCESS;

        default:
            fprintf(stderr,
                    "Corrupt GIF.\n" "reason: unknown code.\n");
            return SIXEL_FAILED;
        }
    }

    return SIXEL_SUCCESS;
}


int
load_gif(unsigned char *buffer,
         int size,
         unsigned char *bgcolor,
         int reqcolors,
         int fuse_palette,
         int fstatic,
         int loop_control,
         sixel_load_image_function /* in */     fn_load,      /* callback */
         void                      /* in/out */ *context      /* private data for callback */
)
{
    stbi__context s;
    stbi__gif g;
    int ret;
    sixel_frame_t *frame;

    frame = sixel_frame_create();
    if (frame == NULL) {
        return SIXEL_FAILED;
    }
    start_mem(&s, buffer, size);
    memset(&g, 0, sizeof(g));
    ret = load_gif_header(&s, &g);
    if (ret != SIXEL_SUCCESS) {
        goto end;
    }
    frame->width = g.w,
    frame->height = g.h,
    g.out = (stbi_uc *)malloc(g.w * g.h);
    if (g.out == NULL) {
        goto end;
    }

    frame->loop_count = 0;

    for (;;) { /* per loop */

        frame->frame_no = 0;

        stbi__rewind(&s);
        ret = load_gif_header(&s, &g);
        if (ret != 0) {
            goto end;
        }

        g.is_terminated = 0;

        for (;;) { /* per frame */
            ret = gif_load_next(&s, &g, bgcolor);
            if (ret != 0) {
                goto end;
            }
            if (g.is_terminated) {
                break;
            }

            ret = init_gif_frame(frame, &g, bgcolor, reqcolors, fuse_palette);
            if (ret != 0) {
                goto end;
            }

            ret = fn_load(frame, context);
            if (ret != 0) {
                goto end;
            }

            if (fstatic) {
                break;
            }
            ++frame->frame_no;
        }

        ++frame->loop_count;

        if (g.loop_count == (-1)) {
            break;
        }
        if (loop_control == LOOP_DISABLE || frame->frame_no == 1) {
            break;
        }
        if (loop_control == LOOP_AUTO && frame->loop_count == g.loop_count) {
            break;
        }
    }

end:
    free(g.out);

    return 0;
}

/* emacs, -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set expandtab ts=4 : */
/* EOF */
