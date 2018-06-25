/*
 * This file is derived from "sixel" original version (2014-3-2)
 * http://nanno.dip.jp/softlib/man/rlogin/sixel.tar.gz
 *
 * Initial developer of this file is kmiya@culti.
 *
 * He distributes it under very permissive license which permits
 * useing, copying, modification, redistribution, and all other
 * public activities without any restrictions.
 *
 * He declares this is compatible with MIT/BSD/GPL.
 *
 * Hayaki Saito <saitoha@me.com> modified this and re-licensed
 * it to the MIT license.
 */
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>   /* isdigit */
#include <string.h>  /* memcpy */

#if defined(HAVE_INTTYPES_H)
# include <inttypes.h>
#endif

#include <sixel.h>
#include "output.h"
#include "debug.h"

#define SIXEL_RGB(r, g, b) (((r) << 16) + ((g) << 8) +  (b))

#define PALVAL(n,a,m) (((n) * (a) + ((m) / 2)) / (m))

#define SIXEL_XRGB(r,g,b) SIXEL_RGB(PALVAL(r, 255, 100), PALVAL(g, 255, 100), PALVAL(b, 255, 100))

#define DECSIXEL_PARAMS_MAX 16

static int const sixel_default_color_table[] = {
    SIXEL_XRGB(0,  0,  0),   /*  0 Black    */
    SIXEL_XRGB(20, 20, 80),  /*  1 Blue     */
    SIXEL_XRGB(80, 13, 13),  /*  2 Red      */
    SIXEL_XRGB(20, 80, 20),  /*  3 Green    */
    SIXEL_XRGB(80, 20, 80),  /*  4 Magenta  */
    SIXEL_XRGB(20, 80, 80),  /*  5 Cyan     */
    SIXEL_XRGB(80, 80, 20),  /*  6 Yellow   */
    SIXEL_XRGB(53, 53, 53),  /*  7 Gray 50% */
    SIXEL_XRGB(26, 26, 26),  /*  8 Gray 25% */
    SIXEL_XRGB(33, 33, 60),  /*  9 Blue*    */
    SIXEL_XRGB(60, 26, 26),  /* 10 Red*     */
    SIXEL_XRGB(33, 60, 33),  /* 11 Green*   */
    SIXEL_XRGB(60, 33, 60),  /* 12 Magenta* */
    SIXEL_XRGB(33, 60, 60),  /* 13 Cyan*    */
    SIXEL_XRGB(60, 60, 33),  /* 14 Yellow*  */
    SIXEL_XRGB(80, 80, 80),  /* 15 Gray 75% */
};

typedef struct image_buffer {
    sixel_index_t *data;
    int width;
    int height;
    int palette[SIXEL_PALETTE_MAX];
    int ncolors;
} image_buffer_t;

typedef enum parse_state {
    PS_GROUND     = 0,
    PS_ESC        = 1,  /* ESC */
    PS_DCS        = 2,  /* DCS Device Control String Introducer \033P P...P I...I F */
    PS_DECSIXEL   = 3,  /* DECSIXEL body part ", $, -, ? ... ~ */
    PS_DECGRA     = 4,  /* DECGRA Set Raster Attributes " Pan; Pad; Ph; Pv */
    PS_DECGRI     = 5,  /* DECGRI Graphics Repeat Introducer ! Pn Ch */
    PS_DECGCI     = 6   /* DECGCI Graphics Color Introducer # Pc; Pu; Px; Py; Pz */
} parse_state_t;

typedef struct parser_context {
    parse_state_t state;
    int pos_x;
    int pos_y;
    int max_x;
    int max_y;
    int attributed_pan;
    int attributed_pad;
    int attributed_ph;
    int attributed_pv;
    int repeat_count;
    int color_index;
    int bgindex;
    int param;
    int nparams;
    int params[DECSIXEL_PARAMS_MAX];
} parser_context_t;


/*
 * Primary color hues:
 *  blue:    0 degrees
 *  red:   120 degrees
 *  green: 240 degrees
 */
static int
hls_to_rgb(int hue, int lum, int sat)
{
    double min, max;
    int r, g, b;

    if (sat == 0) {
        r = g = b = lum;
    }

    /* https://wikimedia.org/api/rest_v1/media/math/render/svg/17e876f7e3260ea7fed73f69e19c71eb715dd09d */
    max = lum + sat * (1.0 - (lum > 50 ? (((lum << 2) / 100.0) - 1.0): - (2 * (lum / 100.0) - 1.0))) / 2.0;

    /* https://wikimedia.org/api/rest_v1/media/math/render/svg/f6721b57985ad83db3d5b800dc38c9980eedde1d */
    min = lum - sat * (1.0 - (lum > 50 ? (((lum << 2) / 100.0) - 1.0): - (2 * (lum / 100.0) - 1.0))) / 2.0;

    /* sixel hue color ring is roteted -120 degree from nowdays general one. */
    hue = (hue + 240) % 360;

    /* https://wikimedia.org/api/rest_v1/media/math/render/svg/937e8abdab308a22ff99de24d645ec9e70f1e384 */
    switch (hue / 60) {
    case 0:  /* 0 <= hue < 60 */
        r = max;
        g = (min + (max - min) * (hue / 60.0));
        b = min;
        break;
    case 1:  /* 60 <= hue < 120 */
        r = min + (max - min) * ((120 - hue) / 60.0);
        g = max;
        b = min;
        break;
    case 2:  /* 120 <= hue < 180 */
        r = min;
        g = max;
        b = (min + (max - min) * ((hue - 120) / 60.0));
        break;
    case 3:  /* 180 <= hue < 240 */
        r = min;
        g = (min + (max - min) * ((240 - hue) / 60.0));
        b = max;
        break;
    case 4:  /* 240 <= hue < 300 */
        r = (min + (max - min) * ((hue - 240) / 60.0));
        g = min;
        b = max;
        break;
    case 5:  /* 300 <= hue < 360 */
        r = max;
        g = min;
        b = (min + (max - min) * ((360 - hue) / 60.0));
        break;
    default:
#if HAVE___BUILTIN_UNREACHABLE
        __builtin_unreachable();
#endif
        break;
    }

    return SIXEL_XRGB(r, g, b);
}


static SIXELSTATUS
image_buffer_init(
    image_buffer_t     *image,
    int                 width,
    int                 height,
    int                 bgindex,
    sixel_allocator_t  *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t size;
    int i;
    int n;
    int r;
    int g;
    int b;

    size = (size_t)(width * height) * sizeof(sixel_index_t);
    image->width = width;
    image->height = height;
    image->data = (sixel_index_t *)sixel_allocator_malloc(allocator, size);
    image->ncolors = 2;

    if (image->data == NULL) {
        sixel_helper_set_additional_message(
            "sixel_deocde_raw: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memset(image->data, bgindex, size);

    /* palette initialization */
    for (n = 0; n < 16; n++) {
        image->palette[n] = sixel_default_color_table[n];
    }

    /* colors 16-231 are a 6x6x6 color cube */
    for (r = 0; r < 6; r++) {
        for (g = 0; g < 6; g++) {
            for (b = 0; b < 6; b++) {
                image->palette[n++] = SIXEL_RGB(r * 51, g * 51, b * 51);
            }
        }
    }

    /* colors 232-255 are a grayscale ramp, intentionally leaving out */
    for (i = 0; i < 24; i++) {
        image->palette[n++] = SIXEL_RGB(i * 11, i * 11, i * 11);
    }

    for (; n < SIXEL_PALETTE_MAX; n++) {
        image->palette[n] = SIXEL_RGB(0, 0, 0);
    }

    status = SIXEL_OK;

end:
    return status;
}


static SIXELSTATUS
image_buffer_resize(
    image_buffer_t     *image,
    int                 width,
    int                 height,
    int                 bgindex,
    sixel_allocator_t  *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t size;
    sixel_index_t *alt_buffer;
    int n;
    int min_height;

    size = (size_t)(width * height) * sizeof(sixel_index_t);
    alt_buffer = (sixel_index_t *)sixel_allocator_malloc(allocator, size);
    if (alt_buffer == NULL) {
        /* free source image */
        sixel_allocator_free(allocator, image->data);
        image->data = NULL;
        sixel_helper_set_additional_message(
            "image_buffer_resize: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    min_height = height > image->height ? image->height: height;
    if (width > image->width) {  /* if width is extended */
        for (n = 0; n < min_height; ++n) {
            /* copy from source image */
            memcpy(alt_buffer + width * n,
                   image->data + image->width * n,
                   (size_t)image->width * sizeof(sixel_index_t));
            /* fill extended area with background color */
            memset(alt_buffer + width * n + image->width,
                   bgindex,
                   (size_t)(width - image->width) * sizeof(sixel_index_t));
        }
    } else {
        for (n = 0; n < min_height; ++n) {
            /* copy from source image */
            memcpy(alt_buffer + width * n,
                   image->data + image->width * n,
                   (size_t)width * sizeof(sixel_index_t));
        }
    }

    if (height > image->height) {  /* if height is extended */
        /* fill extended area with background color */
        memset(alt_buffer + width * image->height,
               bgindex,
               (size_t)(width * (height - image->height)) * sizeof(sixel_index_t));
    }

    /* free source image */
    sixel_allocator_free(allocator, image->data);

    image->data = alt_buffer;
    image->width = width;
    image->height = height;

    status = SIXEL_OK;

end:
    return status;
}


static SIXELSTATUS
parser_context_init(parser_context_t *context)
{
    SIXELSTATUS status = SIXEL_FALSE;

    context->state = PS_GROUND;
    context->pos_x = 0;
    context->pos_y = 0;
    context->max_x = 0;
    context->max_y = 0;
    context->attributed_pan = 2;
    context->attributed_pad = 1;
    context->attributed_ph = 0;
    context->attributed_pv = 0;
    context->repeat_count = 1;
    context->color_index = 15;
    context->bgindex = 0;
    context->nparams = 0;
    context->param = 0;

    status = SIXEL_OK;

    return status;
}


static int
bits_height(int bits) {
    static int height[] = {
        /* ? */ 0,
        /* @ */ 1,
        /* A */ 2, /* B */ 2,
        /* C */ 3, /* D */ 3, /* E */ 3, /* F */ 3,
        /* G */ 4, /* H */ 4, /* I */ 4, /* J */ 4,
        /* K */ 4, /* L */ 4, /* M */ 4, /* N */ 4,
        /* O */ 5, /* P */ 5, /* Q */ 5, /* R */ 5,
        /* S,*/ 5, /* T */ 5, /* U */ 5, /* V */ 5,
        /* W */ 5, /* X */ 5, /* Y */ 5, /* Z */ 5,
        /* [ */ 5, /* \ */ 5, /* ] */ 5, /* ^ */ 5,
        /* _ */ 5, /* ` */ 5, /* a */ 5, /* b */ 5,
        /* c */ 5, /* d */ 5, /* e */ 5, /* f */ 5,
        /* g */ 5, /* h */ 5, /* i */ 5, /* j */ 5,
        /* k */ 5, /* l */ 5, /* m */ 5, /* n */ 5,
        /* o */ 5, /* p */ 5, /* q */ 5, /* r */ 5,
        /* s */ 5, /* t */ 5, /* u */ 5, /* v */ 5,
        /* w */ 5, /* x */ 5, /* y */ 5, /* z */ 5,
        /* { */ 5, /* | */ 5, /* } */ 5, /* ~ */ 5
    };

    return height[bits];
}


/* convert sixel data into indexed pixel bytes and palette data */
static SIXELSTATUS
sixel_decode_raw_impl(
    unsigned char     *p,         /* sixel bytes */
    int                len,       /* size of sixel bytes */
    image_buffer_t    *image,
    parser_context_t  *context,
    sixel_allocator_t *allocator) /* allocator object */
{
    SIXELSTATUS status = SIXEL_FALSE;
    int n;
    int i;
    int y;
    int bits;
    int sixel_vertical_mask;
    int sx;
    int sy;
    int c;
    int pos;
    unsigned char *p0 = p;

    while (p < p0 + len) {
        switch (context->state) {
        case PS_GROUND:
            switch (*p) {
            case 0x1b:
                context->state = PS_ESC;
                p++;
                break;
            case 0x90:
                context->state = PS_DCS;
                p++;
                break;
            case 0x9c:
                p++;
                goto finalize;
            default:
                p++;
                break;
            }
            break;

        case PS_ESC:
            switch (*p) {
            case '\\':
            case 0x9c:
                p++;
                goto finalize;
            case 'P':
                context->param = -1;
                context->state = PS_DCS;
                p++;
                break;
            default:
                p++;
                break;
            }
            break;

        case PS_DCS:
            switch (*p) {
            case 0x1b:
                context->state = PS_ESC;
                p++;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                if (context->param < 0) {
                    context->param = 0;
                }
                context->param = context->param * 10 + *p - '0';
                p++;
                break;
            case ';':
                if (context->param < 0) {
                    context->param = 0;
                }
                if (context->nparams < DECSIXEL_PARAMS_MAX) {
                    context->params[context->nparams++] = context->param;
                }
                context->param = 0;
                p++;
                break;
            case 'q':
                if (context->param >= 0 && context->nparams < DECSIXEL_PARAMS_MAX) {
                    context->params[context->nparams++] = context->param;
                }
                if (context->nparams > 0) {
                    /* Pn1 */
                    switch (context->params[0]) {
                    case 0:
                    case 1:
                        context->attributed_pad = 2;
                        break;
                    case 2:
                        context->attributed_pad = 5;
                        break;
                    case 3:
                    case 4:
                        context->attributed_pad = 4;
                        break;
                    case 5:
                    case 6:
                        context->attributed_pad = 3;
                        break;
                    case 7:
                    case 8:
                        context->attributed_pad = 2;
                        break;
                    case 9:
                        context->attributed_pad = 1;
                        break;
                    default:
                        context->attributed_pad = 2;
                        break;
                    }
                }

                if (context->nparams > 2) {
                    /* Pn3 */
                    if (context->params[2] == 0) {
                        context->params[2] = 10;
                    }
                    context->attributed_pan = context->attributed_pan * context->params[2] / 10;
                    context->attributed_pad = context->attributed_pad * context->params[2] / 10;
                    if (context->attributed_pan <= 0) {
                        context->attributed_pan = 1;
                    }
                    if (context->attributed_pad <= 0) {
                        context->attributed_pad = 1;
                    }
                }
                context->nparams = 0;
                context->state = PS_DECSIXEL;
                p++;
                break;
            default:
                p++;
                break;
            }
            break;

        case PS_DECSIXEL:
            switch (*p) {
            case '\x1b':
                context->state = PS_ESC;
                p++;
                break;
            case '"':
                context->param = 0;
                context->nparams = 0;
                context->state = PS_DECGRA;
                p++;
                break;
            case '!':
                context->param = 0;
                context->nparams = 0;
                context->state = PS_DECGRI;
                p++;
                break;
            case '#':
                context->param = 0;
                context->nparams = 0;
                context->state = PS_DECGCI;
                p++;
                break;
            case '$':
                /* DECGCR Graphics Carriage Return */
                context->pos_x = 0;
                p++;
                break;
            case '-':
                /* DECGNL Graphics Next Line */
                context->pos_x = 0;
                context->pos_y += 6;
                p++;
                break;
            default:
                if (*p >= '?' && *p <= '~') {  /* sixel characters */
                    if (image->width < (context->pos_x + context->repeat_count) || image->height < (context->pos_y + 6)) {
                        sx = image->width * 2;
                        sy = image->height * 2;
                        while (sx < (context->pos_x + context->repeat_count) || sy < (context->pos_y + 6)) {
                            sx *= 2;
                            sy *= 2;
                        }
                        status = image_buffer_resize(image, sx, sy, context->bgindex, allocator);
                        if (SIXEL_FAILED(status)) {
                            goto end;
                        }
                    }

                    if (context->color_index > image->ncolors) {
                        image->ncolors = context->color_index;
                    }

                    bits = *p - '?';

                    if (bits == 0) {
                        context->pos_x += context->repeat_count;
                    } else {
                        sixel_vertical_mask = 0x01;
                        if (context->repeat_count <= 1) {
                            for (i = 0; i < 6; i++) {
                                if ((bits & sixel_vertical_mask) != 0) {
                                    pos = image->width * (context->pos_y + i) + context->pos_x;
                                    image->data[pos] = (sixel_index_t)context->color_index;
                                    if (context->max_x < context->pos_x) {
                                        context->max_x = context->pos_x;
                                    }
                                }
                                sixel_vertical_mask <<= 1;
                            }
                            context->pos_x += 1;
                        } else {
                            /* context->repeat_count > 1 */
                            for (i = 0; i < 6; i++) {
                                if ((bits & sixel_vertical_mask) != 0) {
                                    c = sixel_vertical_mask << 1;
                                    for (n = 1; (i + n) < 6; n++) {
                                        if ((bits & c) == 0) {
                                            break;
                                        }
                                        c <<= 1;
                                    }
                                    for (y = context->pos_y + i; y < context->pos_y + i + n; ++y) {
                                        for (int g = 0 ; g < context->repeat_count; ++g) {
                                            image->data[image->width * y + context->pos_x + g] = (sixel_index_t)context->color_index;
                                        }
                                    }
                                    i += (n - 1);
                                    sixel_vertical_mask <<= (n - 1);
                                }
                                sixel_vertical_mask <<= 1;
                            }
                            context->pos_x += context->repeat_count;
                            if (context->max_x < (context->pos_x + context->repeat_count - 1)) {
                                context->max_x = context->pos_x + context->repeat_count - 1;
                            }
                        }
                    }
                    context->repeat_count = 1;
                    n = bits_height(bits);
                    if (context->max_y < context->pos_y + n) {
                        context->max_y += context->pos_x + n;
                    }
                }
                p++;
                break;
            }
            break;

        case PS_DECGRA:
            /* DECGRA Set Raster Attributes " Pan; Pad; Ph; Pv */
            switch (*p) {
            case '\x1b':
                context->state = PS_ESC;
                p++;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                context->param = context->param * 10 + *p - '0';
                p++;
                break;
            case ';':
                if (context->nparams < DECSIXEL_PARAMS_MAX) {
                    context->params[context->nparams++] = context->param;
                }
                context->param = 0;
                p++;
                break;
            default:
                if (context->nparams < DECSIXEL_PARAMS_MAX) {
                    context->params[context->nparams++] = context->param;
                }
                if (context->nparams > 0) {
                    context->attributed_pad = context->params[0];
                }
                if (context->nparams > 1) {
                    context->attributed_pan = context->params[1];
                }
                if (context->nparams > 2 && context->params[2] > 0) {
                    context->attributed_ph = context->params[2];
                }
                if (context->nparams > 3 && context->params[3] > 0) {
                    context->attributed_pv = context->params[3];
                }

                if (context->attributed_pan <= 0) {
                    context->attributed_pan = 1;
                }
                if (context->attributed_pad <= 0) {
                    context->attributed_pad = 1;
                }

                if (image->width < context->attributed_ph ||
                        image->height < context->attributed_pv) {
                    sx = context->attributed_ph;
                    if (image->width > context->attributed_ph) {
                        sx = image->width;
                    }

                    sy = context->attributed_pv;
                    if (image->height > context->attributed_pv) {
                        sy = image->height;
                    }

                    status = image_buffer_resize(image, sx, sy, context->bgindex, allocator);
                    if (SIXEL_FAILED(status)) {
                        goto end;
                    }
                }
                context->state = PS_DECSIXEL;
                context->param = 0;
                context->nparams = 0;
            }
            break;

        case PS_DECGRI:
            /* DECGRI Graphics Repeat Introducer ! Pn Ch */
            switch (*p) {
            case '\x1b':
                context->state = PS_ESC;
                p++;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                context->param = context->param * 10 + *p - '0';
                p++;
                break;
            default:
                context->repeat_count = context->param;
                if (context->repeat_count == 0) {
                    context->repeat_count = 1;
                }
                context->state = PS_DECSIXEL;
                context->param = 0;
                context->nparams = 0;
                break;
            }
            break;

        case PS_DECGCI:
            /* DECGCI Graphics Color Introducer # Pc; Pu; Px; Py; Pz */
            switch (*p) {
            case '\x1b':
                context->state = PS_ESC;
                p++;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                context->param = context->param * 10 + *p - '0';
                p++;
                break;
            case ';':
                if (context->nparams < DECSIXEL_PARAMS_MAX) {
                    context->params[context->nparams++] = context->param;
                }
                context->param = 0;
                p++;
                break;
            default:
                context->state = PS_DECSIXEL;
                if (context->nparams < DECSIXEL_PARAMS_MAX) {
                    context->params[context->nparams++] = context->param;
                }
                context->param = 0;

                if (context->nparams > 0) {
                    context->color_index = context->params[0] + 1;
                    if (context->color_index < 0) {
                        context->color_index = 0;
                    } else if (context->color_index >= SIXEL_PALETTE_MAX) {
                        context->color_index = SIXEL_PALETTE_MAX - 1;
                    }
                }

                if (context->nparams > 4) {
                    if (context->params[1] == 1) {
                        /* HLS */
                        if (context->params[2] > 360) {
                            context->params[2] = 360;
                        }
                        if (context->params[3] > 100) {
                            context->params[3] = 100;
                        }
                        if (context->params[4] > 100) {
                            context->params[4] = 100;
                        }
                        image->palette[context->color_index]
                            = hls_to_rgb(context->params[2], context->params[3], context->params[4]);
                    } else if (context->params[1] == 2) {
                        /* RGB */
                        if (context->params[2] > 100) {
                            context->params[2] = 100;
                        }
                        if (context->params[3] > 100) {
                            context->params[3] = 100;
                        }
                        if (context->params[4] > 100) {
                            context->params[4] = 100;
                        }
                        image->palette[context->color_index]
                            = SIXEL_XRGB(context->params[2], context->params[3], context->params[4]);
                    }
                }
                break;
            }
            break;
        default:
            break;
        }
    }

finalize:
    if (++context->max_x < context->attributed_ph) {
        context->max_x = context->attributed_ph;
    }

    if (++context->max_y < context->attributed_pv) {
        context->max_y = context->attributed_pv;
    }

    if (image->width > context->max_x || image->height > context->max_y) {
        status = image_buffer_resize(image, context->max_x, context->max_y, context->bgindex, allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = SIXEL_OK;

end:
    return status;
}


/* convert sixel data into indexed pixel bytes and palette data */
SIXELAPI SIXELSTATUS
sixel_decode_wide(
    unsigned char       /* in */  *p,           /* sixel bytes */
    int                 /* in */  len,          /* size of sixel bytes */
    sixel_index_t       /* out */ **pixels,     /* decoded pixels */
    int                 /* out */ *pwidth,      /* image width */
    int                 /* out */ *pheight,     /* image height */
    unsigned char       /* out */ **palette,    /* RGB palette */
    int                 /* out */ *ncolors,     /* palette size (<= 65535) */
    sixel_allocator_t   /* in */  *allocator)   /* allocator object or null */
{
    SIXELSTATUS status = SIXEL_FALSE;
    parser_context_t context;
    image_buffer_t image;
    int n;

    if (allocator) {
        sixel_allocator_ref(allocator);
    } else {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            allocator = NULL;
            goto end;
        }
    }

    /* parser context initialization */
    status = parser_context_init(&context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    /* buffer initialization */
    status = image_buffer_init(&image, 1, 1, context.bgindex, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_decode_raw_impl(p, len, &image, &context, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    *ncolors = image.ncolors + 1;
    *palette = (unsigned char *)sixel_allocator_malloc(allocator, (size_t)(*ncolors * 3));
    if (palette == NULL) {
        sixel_allocator_free(allocator, image.data);
        sixel_helper_set_additional_message(
            "sixel_deocde_raw: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    for (n = 0; n < *ncolors; ++n) {
        (*palette)[n * 3 + 0] = image.palette[n] >> 16 & 0xff;
        (*palette)[n * 3 + 1] = image.palette[n] >> 8 & 0xff;
        (*palette)[n * 3 + 2] = image.palette[n] & 0xff;
    }

    *pwidth = image.width;
    *pheight = image.height;
    *pixels = image.data;

    status = SIXEL_OK;

end:
    sixel_allocator_unref(allocator);
    return status;
}


/* convert sixel data into indexed pixel bytes and palette data */
SIXELAPI SIXELSTATUS
sixel_decode_raw(
    unsigned char       /* in */  *p,           /* sixel bytes */
    int                 /* in */  len,          /* size of sixel bytes */
    unsigned char       /* out */ **pixels,     /* decoded pixels */
    int                 /* out */ *pwidth,      /* image width */
    int                 /* out */ *pheight,     /* image height */
    unsigned char       /* out */ **palette,    /* ARGB palette */
    int                 /* out */ *ncolors,     /* palette size (<= 256) */
    sixel_allocator_t   /* in */  *allocator)   /* allocator object or null */
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_index_t *wide_pixels = NULL;
    unsigned char *narrow_pixels = NULL;
    int n;

    status = sixel_decode_wide(p, len, &wide_pixels, pwidth, pheight, palette, ncolors, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    narrow_pixels = sixel_allocator_malloc(
        allocator,
        (size_t)(*pwidth * *pheight) * sizeof(unsigned char));
    if (narrow_pixels == NULL) {
        sixel_helper_set_additional_message(
            "sixel_decode_raw_wide: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for (n = 0; n < *pwidth * *pheight; ++n) {
        narrow_pixels[n] = wide_pixels[n];
    }

    *pixels = narrow_pixels;

end:
    sixel_allocator_free(allocator, wide_pixels);
    return status;
}

/* deprecated */
SIXELAPI SIXELSTATUS
sixel_decode(unsigned char              /* in */  *p,         /* sixel bytes */
             int                        /* in */  len,        /* size of sixel bytes */
             unsigned char              /* out */ **pixels,   /* decoded pixels */
             int                        /* out */ *pwidth,    /* image width */
             int                        /* out */ *pheight,   /* image height */
             unsigned char              /* out */ **palette,  /* ARGB palette */
             int                        /* out */ *ncolors,   /* palette size (<= 256) */
             sixel_allocator_function   /* in */  fn_malloc)  /* malloc function */
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_allocator_t *allocator = NULL;

    status = sixel_allocator_new(&allocator, fn_malloc, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        allocator = NULL;
        goto end;
    }

    status = sixel_decode_raw(p, len, pixels, pwidth, pheight, palette, ncolors, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    sixel_allocator_unref(allocator);
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
