/*
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
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

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

#include "config.h"

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif
#if HAVE_TIME_H
# include <time.h>
#endif
#if defined(_WIN32)
# include <windows.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#if HAVE_SPAWN_H
# include <spawn.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#ifdef HAVE_GDK_PIXBUF2
# if HAVE_DIAGNOSTIC_TYPEDEF_REDEFINITION
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wtypedef-redefinition"
# endif
# include <gdk-pixbuf/gdk-pixbuf.h>
# include <gdk-pixbuf/gdk-pixbuf-simple-anim.h>
# if HAVE_DIAGNOSTIC_TYPEDEF_REDEFINITION
#   pragma GCC diagnostic pop
# endif
#endif
#if HAVE_GD
# include <gd.h>
#endif
#if HAVE_LIBPNG
# include <png.h>
#endif  /* HAVE_LIBPNG */
#if HAVE_JPEG
# include <jpeglib.h>
#endif  /* HAVE_JPEG */
#if HAVE_COREGRAPHICS
# include <ApplicationServices/ApplicationServices.h>
# include <ImageIO/ImageIO.h>
#endif  /* HAVE_COREGRAPHICS */
#if HAVE_QUICKLOOK
# include <CoreServices/CoreServices.h>
# include <QuickLook/QuickLook.h>
#endif  /* HAVE_QUICKLOOK */

#if HAVE_QUICKLOOK_THUMBNAILING
CGImageRef
sixel_quicklook_thumbnail_create(CFURLRef url, CGSize max_size);
#endif

#if !defined(HAVE_MEMCPY)
# define memcpy(d, s, n) (bcopy ((s), (d), (n)))
#endif

#include <sixel.h>
#include "loader.h"
#include "frame.h"
#include "chunk.h"
#include "frompnm.h"
#include "fromgif.h"
#include "allocator.h"

#define SIXEL_THUMBNAILER_DEFAULT_SIZE 512

static int loader_trace_enabled;
static int thumbnailer_size_hint = SIXEL_THUMBNAILER_DEFAULT_SIZE;

#if HAVE_POSIX_SPAWNP
extern char **environ;
#endif

/*
 * sixel_helper_set_loader_trace
 *
 * Toggle verbose loader tracing so debugging output can be collected.
 *
 * Arguments:
 *     enable - non-zero enables tracing, zero disables it.
 */
void
sixel_helper_set_loader_trace(int enable)
{
    loader_trace_enabled = enable ? 1 : 0;
}

/*
 * sixel_helper_set_thumbnail_size_hint
 *
 * Record the caller's preferred maximum thumbnail dimension.
 *
 * Arguments:
 *     size - requested dimension in pixels; non-positive resets to default.
 */
void
sixel_helper_set_thumbnail_size_hint(int size)
{
    if (size > 0) {
        thumbnailer_size_hint = size;
    } else {
        thumbnailer_size_hint = SIXEL_THUMBNAILER_DEFAULT_SIZE;
    }
}

/*
 * loader_trace_message
 *
 * Emit a formatted trace message when verbose loader tracing is enabled.
 *
 * Arguments:
 *     format - printf-style message template.
 *     ...    - arguments consumed according to the format string.
 */
#if HAVE_UNISTD_H && HAVE_SYS_WAIT_H && HAVE_FORK
static void
loader_trace_message(char const *format, ...)
{
    va_list args;

    if (!loader_trace_enabled) {
        return;
    }

    fprintf(stderr, "libsixel: ");

    va_start(args, format);
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wformat-nonliteral"
#elif defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    vfprintf(stderr, format, args);
#if defined(__clang__)
# pragma clang diagnostic pop
#elif defined(__GNUC__)
# pragma GCC diagnostic pop
#endif
    va_end(args);

    fprintf(stderr, "\n");
}

static int
thumbnailer_format_append_char(char *buffer,
                               size_t capacity,
                               size_t *position,
                               char ch,
                               int *truncated)
{
    if (buffer == NULL || position == NULL) {
        return 0;
    }

    if (*position + 1 < capacity) {
        buffer[*position] = ch;
        *position += 1;
        buffer[*position] = '\0';
    } else {
        if (capacity > 0) {
            buffer[capacity - 1] = '\0';
        }
        if (truncated != NULL) {
            *truncated = 1;
        }
    }

    return 1;
}

static int
thumbnailer_format_append_text(char *buffer,
                               size_t capacity,
                               size_t *position,
                               char const *text,
                               int *truncated)
{
    size_t length;
    size_t copy_length;

    if (buffer == NULL || position == NULL) {
        return 0;
    }

    if (text == NULL) {
        text = "(null)";
    }

    length = strlen(text);
    copy_length = length;

    if (*position + copy_length >= capacity) {
        if (capacity <= *position) {
            copy_length = 0;
        } else {
            copy_length = capacity - 1 - *position;
        }
        if (truncated != NULL) {
            *truncated = 1;
        }
    }

    if (copy_length > 0) {
        memcpy(buffer + *position, text, copy_length);
        *position += copy_length;
        buffer[*position] = '\0';
    } else if (*position < capacity) {
        buffer[*position] = '\0';
    } else if (capacity > 0) {
        buffer[capacity - 1] = '\0';
    }

    return 1;
}

static int
thumbnailer_format_append_decimal(char *buffer,
                                  size_t capacity,
                                  size_t *position,
                                  int value,
                                  int *truncated)
{
    char digits[16];
    size_t index;
    size_t length;
    unsigned int magnitude;
    int negative;

    if (buffer == NULL || position == NULL) {
        return 0;
    }

    index = 0;
    magnitude = 0;
    negative = 0;

    if (value < 0) {
        negative = 1;
        magnitude = (unsigned int)(-value);
    } else {
        magnitude = (unsigned int)value;
    }

    do {
        digits[index] = (char)('0' + (magnitude % 10));
        index += 1;
        magnitude /= 10;
    } while (magnitude != 0);

    if (negative) {
        digits[index] = '-';
        index += 1;
    }

    length = index;

    while (index > 0) {
        index -= 1;
        if (!thumbnailer_format_append_char(buffer,
                                            capacity,
                                            position,
                                            digits[index],
                                            truncated)) {
            return 0;
        }
    }

    if (length == 0) {
        if (!thumbnailer_format_append_char(buffer,
                                            capacity,
                                            position,
                                            '0',
                                            truncated)) {
            return 0;
        }
    }

    return 1;
}

static int
thumbnailer_safe_format(char *buffer,
                        size_t capacity,
                        char const *format,
                        ...)
{
    va_list args;
    size_t position;
    char const *ptr;
    int truncated;
    char const *text;
    int value;

    if (buffer == NULL || capacity == 0 || format == NULL) {
        return 0;
    }

    position = 0;
    truncated = 0;
    buffer[0] = '\0';

    va_start(args, format);
    ptr = format;
    while (ptr != NULL && ptr[0] != '\0') {
        if (ptr[0] != '%') {
            if (!thumbnailer_format_append_char(buffer,
                                                capacity,
                                                &position,
                                                ptr[0],
                                                &truncated)) {
                va_end(args);
                return 0;
            }
            ptr += 1;
            continue;
        }

        ptr += 1;
        if (ptr[0] == '%') {
            if (!thumbnailer_format_append_char(buffer,
                                                capacity,
                                                &position,
                                                '%',
                                                &truncated)) {
                va_end(args);
                return 0;
            }
            ptr += 1;
            continue;
        }

        if (ptr[0] == 's') {
            text = va_arg(args, char const *);
            if (!thumbnailer_format_append_text(buffer,
                                                capacity,
                                                &position,
                                                text,
                                                &truncated)) {
                va_end(args);
                return 0;
            }
            ptr += 1;
            continue;
        }

        if (ptr[0] == 'd') {
            value = va_arg(args, int);
            if (!thumbnailer_format_append_decimal(buffer,
                                                   capacity,
                                                   &position,
                                                   value,
                                                   &truncated)) {
                va_end(args);
                return 0;
            }
            ptr += 1;
            continue;
        }

        va_end(args);
        return 0;
    }
    va_end(args);

    if (truncated != 0 && capacity > 0) {
        buffer[capacity - 1] = '\0';
    }

    return 1;
}

/*
 * thumbnailer_sleep_briefly
 *
 * Yield the CPU for a short duration so child polling loops avoid busy
 * waiting.
 *
 */
static void
thumbnailer_sleep_briefly(void)
{
#if HAVE_NANOSLEEP
    struct timespec ts;
#endif

#if HAVE_NANOSLEEP
    ts.tv_sec = 0;
    ts.tv_nsec = 10000000L;
    nanosleep(&ts, NULL);
#elif defined(_WIN32)
    Sleep(10);
#else
    (void)usleep(10000);
#endif
}

#endif  /* HAVE_UNISTD_H && HAVE_SYS_WAIT_H && HAVE_FORK */

static void
loader_trace_try(char const *name)
{
    if (loader_trace_enabled) {
        fprintf(stderr, "libsixel: trying %s loader\n", name);
    }
}

static void
loader_trace_result(char const *name, SIXELSTATUS status)
{
    if (!loader_trace_enabled) {
        return;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr, "libsixel: loader %s succeeded\n", name);
    } else {
        fprintf(stderr, "libsixel: loader %s failed (%s)\n",
                name, sixel_helper_format_error(status));
    }
}

sixel_allocator_t *stbi_allocator;

void *
stbi_malloc(size_t n)
{
    return sixel_allocator_malloc(stbi_allocator, n);
}

void *
stbi_realloc(void *p, size_t n)
{
    return sixel_allocator_realloc(stbi_allocator, p, n);
}

void
stbi_free(void *p)
{
    sixel_allocator_free(stbi_allocator, p);
}

#define STBI_MALLOC stbi_malloc
#define STBI_REALLOC stbi_realloc
#define STBI_FREE stbi_free

#define STBI_NO_STDIO 1
#define STB_IMAGE_IMPLEMENTATION 1
#define STBI_FAILURE_USERMSG 1
#if defined(_WIN32)
# define STBI_NO_THREAD_LOCALS 1  /* no tls */
#endif
#define STBI_NO_GIF
#define STBI_NO_PNM

#if HAVE_DIAGNOSTIC_SIGN_CONVERSION
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#if HAVE_DIAGNOSTIC_STRICT_OVERFLOW
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wstrict-overflow"
#endif
#if HAVE_DIAGNOSTIC_SWITCH_DEFAULT
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wswitch-default"
#endif
#if HAVE_DIAGNOSTIC_SHADOW
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wshadow"
#endif
#if HAVE_DIAGNOSTIC_DOUBLE_PROMOTION
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif
# if HAVE_DIAGNOSTIC_UNUSED_FUNCTION
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-function"
#endif
# if HAVE_DIAGNOSTIC_UNUSED_BUT_SET_VARIABLE
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#include "stb_image.h"
#if HAVE_DIAGNOSTIC_UNUSED_BUT_SET_VARIABLE
# pragma GCC diagnostic pop
#endif
#if HAVE_DIAGNOSTIC_UNUSED_FUNCTION
# pragma GCC diagnostic pop
#endif
#if HAVE_DIAGNOSTIC_DOUBLE_PROMOTION
# pragma GCC diagnostic pop
#endif
#if HAVE_DIAGNOSTIC_SHADOW
# pragma GCC diagnostic pop
#endif
#if HAVE_DIAGNOSTIC_SWITCH_DEFAULT
# pragma GCC diagnostic pop
#endif
#if HAVE_DIAGNOSTIC_STRICT_OVERFLOW
# pragma GCC diagnostic pop
#endif
#if HAVE_DIAGNOSTIC_SIGN_CONVERSION
# pragma GCC diagnostic pop
#endif


# if HAVE_JPEG
/* import from @uobikiemukot's sdump loader.h */
static SIXELSTATUS
load_jpeg(unsigned char **result,
          unsigned char *data,
          size_t datasize,
          int *pwidth,
          int *pheight,
          int *ppixelformat,
          sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_JPEG_ERROR;
    JDIMENSION row_stride;
    size_t size;
    JSAMPARRAY buffer;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr pub;

    cinfo.err = jpeg_std_error(&pub);

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, datasize);
    jpeg_read_header(&cinfo, TRUE);

    /* disable colormap (indexed color), grayscale -> rgb */
    cinfo.quantize_colors = FALSE;
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    if (cinfo.output_components != 3) {
        sixel_helper_set_additional_message(
            "load_jpeg: unknown pixel format.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *ppixelformat = SIXEL_PIXELFORMAT_RGB888;

    if (cinfo.output_width > INT_MAX || cinfo.output_height > INT_MAX) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    *pwidth = (int)cinfo.output_width;
    *pheight = (int)cinfo.output_height;

    size = (size_t)(*pwidth * *pheight * cinfo.output_components);
    *result = (unsigned char *)sixel_allocator_malloc(allocator, size);
    if (*result == NULL) {
        sixel_helper_set_additional_message(
            "load_jpeg: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    row_stride = cinfo.output_width * (unsigned int)cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        if (cinfo.err->num_warnings > 0) {
            sixel_helper_set_additional_message(
                "jpeg_read_scanlines: error/warining occuered.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        memcpy(*result + (cinfo.output_scanline - 1) * row_stride, buffer[0], row_stride);
    }

    status = SIXEL_OK;

end:
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return status;
}
# endif  /* HAVE_JPEG */


# if HAVE_LIBPNG
static void
read_png(png_structp png_ptr,
         png_bytep data,
         png_size_t length)
{
    sixel_chunk_t *pchunk = (sixel_chunk_t *)png_get_io_ptr(png_ptr);
    if (length > pchunk->size) {
        length = pchunk->size;
    }
    if (length > 0) {
        memcpy(data, pchunk->buffer, length);
        pchunk->buffer += length;
        pchunk->size -= length;
    }
}


static void
read_palette(png_structp png_ptr,
             png_infop info_ptr,
             unsigned char *palette,
             int ncolors,
             png_color *png_palette,
             png_color_16 *pbackground,
             int *transparent)
{
    png_bytep trans = NULL;
    int num_trans = 0;
    int i;

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, NULL);
    }
    if (num_trans > 0) {
        *transparent = trans[0];
    }
    for (i = 0; i < ncolors; ++i) {
        if (pbackground && i < num_trans) {
            palette[i * 3 + 0] = ((0xff - trans[i]) * pbackground->red
                                   + trans[i] * png_palette[i].red) >> 8;
            palette[i * 3 + 1] = ((0xff - trans[i]) * pbackground->green
                                   + trans[i] * png_palette[i].green) >> 8;
            palette[i * 3 + 2] = ((0xff - trans[i]) * pbackground->blue
                                   + trans[i] * png_palette[i].blue) >> 8;
        } else {
            palette[i * 3 + 0] = png_palette[i].red;
            palette[i * 3 + 1] = png_palette[i].green;
            palette[i * 3 + 2] = png_palette[i].blue;
        }
    }
}

jmp_buf jmpbuf;

/* libpng error handler */
static void
png_error_callback(png_structp png_ptr, png_const_charp error_message)
{
    (void) png_ptr;

    sixel_helper_set_additional_message(error_message);
#if HAVE_SETJMP && HAVE_LONGJMP
    longjmp(jmpbuf, 1);
#endif  /* HAVE_SETJMP && HAVE_LONGJMP */
}


static SIXELSTATUS
load_png(unsigned char      /* out */ **result,
         unsigned char      /* in */  *buffer,
         size_t             /* in */  size,
         int                /* out */ *psx,
         int                /* out */ *psy,
         unsigned char      /* out */ **ppalette,
         int                /* out */ *pncolors,
         int                /* in */  reqcolors,
         int                /* out */ *pixelformat,
         unsigned char      /* out */ *bgcolor,
         int                /* out */ *transparent,
         sixel_allocator_t  /* in */  *allocator)
{
    SIXELSTATUS status;
    sixel_chunk_t read_chunk;
    png_uint_32 bitdepth;
    png_uint_32 png_status;
    png_structp png_ptr;
    png_infop info_ptr;
#ifdef HAVE_DIAGNOSTIC_CLOBBERED
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wclobbered"
#endif
    unsigned char **rows = NULL;
    png_color *png_palette = NULL;
    png_color_16 background;
    png_color_16p default_background;
    png_uint_32 width;
    png_uint_32 height;
    int i;
    int depth;

#if HAVE_SETJMP && HAVE_LONGJMP
    if (setjmp(jmpbuf) != 0) {
        sixel_allocator_free(allocator, *result);
        *result = NULL;
        status = SIXEL_PNG_ERROR;
        goto cleanup;
    }
#endif  /* HAVE_SETJMP && HAVE_LONGJMP */

    status = SIXEL_FALSE;
    *result = NULL;

    png_ptr = png_create_read_struct(
        PNG_LIBPNG_VER_STRING, NULL, &png_error_callback, NULL);
    if (!png_ptr) {
        sixel_helper_set_additional_message(
            "png_create_read_struct() failed.");
        status = SIXEL_PNG_ERROR;
        goto cleanup;
    }

    /*
     * The minimum valid PNG is 67 bytes.
     * https://garethrees.org/2007/11/14/pngcrush/
     */
    if (size < 67) {
        sixel_helper_set_additional_message("PNG data too small to be valid!");
        status = SIXEL_PNG_ERROR;
        goto cleanup;
    }

#if HAVE_SETJMP
    if (setjmp(png_jmpbuf(png_ptr)) != 0) {
        sixel_allocator_free(allocator, *result);
        *result = NULL;
        status = SIXEL_PNG_ERROR;
        goto cleanup;
    }
#endif  /* HAVE_SETJMP */

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        sixel_helper_set_additional_message(
            "png_create_info_struct() failed.");
        status = SIXEL_PNG_ERROR;
        png_destroy_read_struct(&png_ptr, (png_infopp)0, (png_infopp)0);
        goto cleanup;
    }
    read_chunk.buffer = buffer;
    read_chunk.size = size;

    png_set_read_fn(png_ptr,(png_voidp)&read_chunk, read_png);
    png_read_info(png_ptr, info_ptr);

    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);

    if (width > INT_MAX || height > INT_MAX) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }

    *psx = (int)width;
    *psy = (int)height;

    bitdepth = png_get_bit_depth(png_ptr, info_ptr);
    if (bitdepth == 16) {
#  if HAVE_DEBUG
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
        fprintf(stderr, "stripping to 8bit...\n");
#  endif
        png_set_strip_16(png_ptr);
        bitdepth = 8;
    }

    if (bgcolor) {
#  if HAVE_DEBUG
        fprintf(stderr, "background color is specified [%02x, %02x, %02x]\n",
                bgcolor[0], bgcolor[1], bgcolor[2]);
#  endif
        background.red = bgcolor[0];
        background.green = bgcolor[1];
        background.blue = bgcolor[2];
        background.gray = (bgcolor[0] + bgcolor[1] + bgcolor[2]) / 3;
    } else if (png_get_bKGD(png_ptr, info_ptr, &default_background) == PNG_INFO_bKGD) {
        memcpy(&background, default_background, sizeof(background));
#  if HAVE_DEBUG
        fprintf(stderr, "background color is found [%02x, %02x, %02x]\n",
                background.red, background.green, background.blue);
#  endif
    } else {
        background.red = 0;
        background.green = 0;
        background.blue = 0;
        background.gray = 0;
    }

    switch (png_get_color_type(png_ptr, info_ptr)) {
    case PNG_COLOR_TYPE_PALETTE:
#  if HAVE_DEBUG
        fprintf(stderr, "paletted PNG(PNG_COLOR_TYPE_PALETTE)\n");
#  endif
        png_status = png_get_PLTE(png_ptr, info_ptr,
                                  &png_palette, pncolors);
        if (png_status != PNG_INFO_PLTE || png_palette == NULL) {
            sixel_helper_set_additional_message(
                "PLTE chunk not found");
            status = SIXEL_PNG_ERROR;
            goto cleanup;
        }
#  if HAVE_DEBUG
        fprintf(stderr, "palette colors: %d\n", *pncolors);
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
#  endif
        if (ppalette == NULL || *pncolors > reqcolors) {
#  if HAVE_DEBUG
            fprintf(stderr, "detected more colors than reqired(>%d).\n",
                    reqcolors);
            fprintf(stderr, "expand to RGB format...\n");
#  endif
            png_set_background(png_ptr, &background,
                               PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
            png_set_palette_to_rgb(png_ptr);
            png_set_strip_alpha(png_ptr);
            *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        } else {
            switch (bitdepth) {
            case 1:
                *ppalette = (unsigned char *)sixel_allocator_malloc(allocator, (size_t)*pncolors * 3);
                if (*ppalette == NULL) {
                    sixel_helper_set_additional_message(
                        "load_png: sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors, png_palette, &background, transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL1;
                break;
            case 2:
                *ppalette = (unsigned char *)sixel_allocator_malloc(allocator, (size_t)*pncolors * 3);
                if (*ppalette == NULL) {
                    sixel_helper_set_additional_message(
                        "load_png: sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors, png_palette, &background, transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL2;
                break;
            case 4:
                *ppalette = (unsigned char *)sixel_allocator_malloc(allocator, (size_t)*pncolors * 3);
                if (*ppalette == NULL) {
                    sixel_helper_set_additional_message(
                        "load_png: sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors, png_palette, &background, transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL4;
                break;
            case 8:
                *ppalette = (unsigned char *)sixel_allocator_malloc(allocator, (size_t)*pncolors * 3);
                if (*ppalette == NULL) {
                    sixel_helper_set_additional_message(
                        "load_png: sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors, png_palette, &background, transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL8;
                break;
            default:
                png_set_background(png_ptr, &background,
                                   PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                png_set_palette_to_rgb(png_ptr);
                *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                break;
            }
        }
        break;
    case PNG_COLOR_TYPE_GRAY:
#  if HAVE_DEBUG
        fprintf(stderr, "grayscale PNG(PNG_COLOR_TYPE_GRAY)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
#  endif
        if (1 << bitdepth > reqcolors) {
#  if HAVE_DEBUG
            fprintf(stderr, "detected more colors than reqired(>%d).\n",
                    reqcolors);
            fprintf(stderr, "expand into RGB format...\n");
#  endif
            png_set_background(png_ptr, &background,
                               PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
            png_set_gray_to_rgb(png_ptr);
            *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        } else {
            switch (bitdepth) {
            case 1:
            case 2:
            case 4:
                if (ppalette) {
#  if HAVE_DECL_PNG_SET_EXPAND_GRAY_1_2_4_TO_8
#   if HAVE_DEBUG
                    fprintf(stderr, "expand %u bpp to 8bpp format...\n",
                            (unsigned int)bitdepth);
#   endif
                    png_set_expand_gray_1_2_4_to_8(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_G8;
#  elif HAVE_DECL_PNG_SET_GRAY_1_2_4_TO_8
#   if HAVE_DEBUG
                    fprintf(stderr, "expand %u bpp to 8bpp format...\n",
                            (unsigned int)bitdepth);
#   endif
                    png_set_gray_1_2_4_to_8(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_G8;
#  else
#   if HAVE_DEBUG
                    fprintf(stderr, "expand into RGB format...\n");
#   endif
                    png_set_background(png_ptr, &background,
                                       PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                    png_set_gray_to_rgb(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_RGB888;
#  endif
                } else {
                    png_set_background(png_ptr, &background,
                                       PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                    png_set_gray_to_rgb(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                }
                break;
            case 8:
                if (ppalette) {
                    *pixelformat = SIXEL_PIXELFORMAT_G8;
                } else {
#  if HAVE_DEBUG
                    fprintf(stderr, "expand into RGB format...\n");
#  endif
                    png_set_background(png_ptr, &background,
                                       PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                    png_set_gray_to_rgb(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                }
                break;
            default:
#  if HAVE_DEBUG
                fprintf(stderr, "expand into RGB format...\n");
#  endif
                png_set_background(png_ptr, &background,
                                   PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                png_set_gray_to_rgb(png_ptr);
                *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                break;
            }
        }
        break;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
#  if HAVE_DEBUG
        fprintf(stderr, "grayscale-alpha PNG(PNG_COLOR_TYPE_GRAY_ALPHA)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
        fprintf(stderr, "expand to RGB format...\n");
#  endif
        png_set_background(png_ptr, &background,
                           PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
        png_set_gray_to_rgb(png_ptr);
        *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
#  if HAVE_DEBUG
        fprintf(stderr, "RGBA PNG(PNG_COLOR_TYPE_RGB_ALPHA)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
        fprintf(stderr, "expand to RGB format...\n");
#  endif
        png_set_background(png_ptr, &background,
                           PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
        *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    case PNG_COLOR_TYPE_RGB:
#  if HAVE_DEBUG
        fprintf(stderr, "RGB PNG(PNG_COLOR_TYPE_RGB)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
#  endif
        png_set_background(png_ptr, &background,
                           PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
        *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    default:
        /* unknown format */
        goto cleanup;
    }
    depth = sixel_helper_compute_depth(*pixelformat);
    *result = (unsigned char *)sixel_allocator_malloc(allocator, (size_t)(*psx * *psy * depth));
    if (*result == NULL) {
        sixel_helper_set_additional_message(
            "load_png: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    rows = (unsigned char **)sixel_allocator_malloc(allocator, (size_t)*psy * sizeof(unsigned char *));
    if (rows == NULL) {
        sixel_helper_set_additional_message(
            "load_png: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    switch (*pixelformat) {
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_PAL4:
        for (i = 0; i < *psy; ++i) {
            rows[i] = *result + (depth * *psx * (int)bitdepth + 7) / 8 * i;
        }
        break;
    default:
        for (i = 0; i < *psy; ++i) {
            rows[i] = *result + depth * *psx * i;
        }
        break;
    }

    png_read_image(png_ptr, rows);

    status = SIXEL_OK;

cleanup:
    png_destroy_read_struct(&png_ptr, &info_ptr,(png_infopp)0);

    if (rows != NULL) {
        sixel_allocator_free(allocator, rows);
    }

    return status;
}
#ifdef HAVE_DIAGNOSTIC_CLOBBERED
# pragma GCC diagnostic pop
#endif

# endif  /* HAVE_LIBPNG */


static SIXELSTATUS
load_sixel(unsigned char        /* out */ **result,
           unsigned char        /* in */  *buffer,
           int                  /* in */  size,
           int                  /* out */ *psx,
           int                  /* out */ *psy,
           unsigned char        /* out */ **ppalette,
           int                  /* out */ *pncolors,
           int                  /* in */  reqcolors,
           int                  /* out */ *ppixelformat,
           sixel_allocator_t    /* in */  *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char *p = NULL;
    unsigned char *palette = NULL;
    int colors;
    int i;

    /* sixel */
    status = sixel_decode_raw(buffer, size,
                              &p, psx, psy,
                              &palette, &colors, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (ppalette == NULL || colors > reqcolors) {
        *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
        *result = (unsigned char *)sixel_allocator_malloc(allocator, (size_t)(*psx * *psy * 3));
        if (*result == NULL) {
            sixel_helper_set_additional_message(
                "load_sixel: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (i = 0; i < *psx * *psy; ++i) {
            (*result)[i * 3 + 0] = palette[p[i] * 3 + 0];
            (*result)[i * 3 + 1] = palette[p[i] * 3 + 1];
            (*result)[i * 3 + 2] = palette[p[i] * 3 + 2];
        }
    } else {
        *ppixelformat = SIXEL_PIXELFORMAT_PAL8;
        *result = p;
        *ppalette = palette;
        *pncolors = colors;
        p = NULL;
        palette = NULL;
    }

end:
    sixel_allocator_free(allocator, palette);
    sixel_allocator_free(allocator, p);

    return status;
}


/* detect whether given chunk is sixel stream */
static int
chunk_is_sixel(sixel_chunk_t const *chunk)
{
    unsigned char *p;
    unsigned char *end;

    p = chunk->buffer;
    end = p + chunk->size;

    if (chunk->size < 3) {
        return 0;
    }

    p++;
    if (p >= end) {
        return 0;
    }
    if (*(p - 1) == 0x90 || (*(p - 1) == 0x1b && *p == 0x50)) {
        while (p++ < end) {
            if (*p == 0x71) {
                return 1;
            } else if (*p == 0x18 || *p == 0x1a) {
                return 0;
            } else if (*p < 0x20) {
                continue;
            } else if (*p < 0x30) {
                return 0;
            } else if (*p < 0x40) {
                continue;
            }
        }
    }
    return 0;
}


/* detect whether given chunk is PNM stream */
static int
chunk_is_pnm(sixel_chunk_t const *chunk)
{
    if (chunk->size < 2) {
        return 0;
    }
    if (chunk->buffer[0] == 'P' &&
        chunk->buffer[1] >= '1' &&
        chunk->buffer[1] <= '6') {
        return 1;
    }
    return 0;
}


#if HAVE_LIBPNG
/* detect whether given chunk is PNG stream */
static int
chunk_is_png(sixel_chunk_t const *chunk)
{
    if (chunk->size < 8) {
        return 0;
    }
    if (png_check_sig(chunk->buffer, 8)) {
        return 1;
    }
    return 0;
}
#endif  /* HAVE_LIBPNG */


/* detect whether given chunk is GIF stream */
static int
chunk_is_gif(sixel_chunk_t const *chunk)
{
    if (chunk->size < 6) {
        return 0;
    }
    if (chunk->buffer[0] == 'G' &&
        chunk->buffer[1] == 'I' &&
        chunk->buffer[2] == 'F' &&
        chunk->buffer[3] == '8' &&
        (chunk->buffer[4] == '7' || chunk->buffer[4] == '9') &&
        chunk->buffer[5] == 'a') {
        return 1;
    }
    return 0;
}


#if HAVE_JPEG
/* detect whether given chunk is JPEG stream */
static int
chunk_is_jpeg(sixel_chunk_t const *chunk)
{
    if (chunk->size < 2) {
        return 0;
    }
    if (memcmp("\xFF\xD8", chunk->buffer, 2) == 0) {
        return 1;
    }
    return 0;
}
#endif  /* HAVE_JPEG */

typedef union _fn_pointer {
    sixel_load_image_function fn;
    void *                    p;
} fn_pointer;

/* load images using builtin image loaders */
static SIXELSTATUS
load_with_builtin(
    sixel_chunk_t const       /* in */     *pchunk,      /* image data */
    int                       /* in */     fstatic,      /* static */
    int                       /* in */     fuse_palette, /* whether to use palette if possible */
    int                       /* in */     reqcolors,    /* reqcolors */
    unsigned char             /* in */     *bgcolor,     /* background color */
    int                       /* in */     loop_control, /* one of enum loop_control */
    sixel_load_image_function /* in */     fn_load,      /* callback */
    void                      /* in/out */ *context      /* private data for callback */
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_frame_t *frame = NULL;
    char message[256];
    int nwrite;
    fn_pointer fnp;

    if (chunk_is_sixel(pchunk)) {
        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (pchunk->size > INT_MAX) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        status = load_sixel(&frame->pixels,
                            pchunk->buffer,
                            (int)pchunk->size,
                            &frame->width,
                            &frame->height,
                            fuse_palette ? &frame->palette: NULL,
                            &frame->ncolors,
                            reqcolors,
                            &frame->pixelformat,
                            pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else if (chunk_is_pnm(pchunk)) {
        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (pchunk->size > INT_MAX) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        /* pnm */
        status = load_pnm(pchunk->buffer,
                          (int)pchunk->size,
                          frame->allocator,
                          &frame->pixels,
                          &frame->width,
                          &frame->height,
                          fuse_palette ? &frame->palette: NULL,
                          &frame->ncolors,
                          &frame->pixelformat);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
#if HAVE_JPEG
    else if (chunk_is_jpeg(pchunk)) {
        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = load_jpeg(&frame->pixels,
                           pchunk->buffer,
                           pchunk->size,
                           &frame->width,
                           &frame->height,
                           &frame->pixelformat,
                           pchunk->allocator);

        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
#endif  /* HAVE_JPEG */
#if HAVE_LIBPNG
    else if (chunk_is_png(pchunk)) {
        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = load_png(&frame->pixels,
                          pchunk->buffer,
                          pchunk->size,
                          &frame->width,
                          &frame->height,
                          fuse_palette ? &frame->palette: NULL,
                          &frame->ncolors,
                          reqcolors,
                          &frame->pixelformat,
                          bgcolor,
                          &frame->transparent,
                          pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
#endif  /* HAVE_LIBPNG */
    else if (chunk_is_gif(pchunk)) {
        fnp.fn = fn_load;
        if (pchunk->size > INT_MAX) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        status = load_gif(pchunk->buffer,
                          (int)pchunk->size,
                          bgcolor,
                          reqcolors,
                          fuse_palette,
                          fstatic,
                          loop_control,
                          fnp.p,
                          context,
                          pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        goto end;
    } else {
        stbi__context s;
        int depth;

        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        stbi_allocator = pchunk->allocator;
        if (pchunk->size > INT_MAX) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        stbi__start_mem(&s, pchunk->buffer, (int)pchunk->size);
        frame->pixels = stbi__load_and_postprocess_8bit(&s, &frame->width, &frame->height, &depth, 3);
        if (!frame->pixels) {
            sixel_helper_set_additional_message(stbi_failure_reason());
            status = SIXEL_STBI_ERROR;
            goto end;
        }
        frame->loop_count = 1;

        switch (depth) {
        case 1:
        case 3:
        case 4:
            frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
            break;
        default:
            nwrite = sprintf(message,
                             "load_with_builtin() failed.\n"
                             "reason: unknown pixel-format.(depth: %d)\n",
                             depth);
            if (nwrite > 0) {
                sixel_helper_set_additional_message(message);
            }
            goto end;
        }
    }

    status = sixel_frame_strip_alpha(frame, bgcolor);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = fn_load(frame, context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
    sixel_frame_unref(frame);

    return status;
}


#ifdef HAVE_GDK_PIXBUF2
/*
 * Loader backed by gdk-pixbuf2. The entire animation is consumed via
 * GdkPixbufLoader, each frame is copied into a temporary buffer and forwarded as
 * a sixel_frame_t. Loop attributes provided by gdk-pixbuf are reconciled with
 * libsixel's loop control settings.
 */
static SIXELSTATUS
load_with_gdkpixbuf(
    sixel_chunk_t const       /* in */     *pchunk,      /* image data */
    int                       /* in */     fstatic,      /* static */
    int                       /* in */     fuse_palette, /* whether to use palette if possible */
    int                       /* in */     reqcolors,    /* reqcolors */
    unsigned char             /* in */     *bgcolor,     /* background color */
    int                       /* in */     loop_control, /* one of enum loop_control */
    sixel_load_image_function /* in */     fn_load,      /* callback */
    void                      /* in/out */ *context      /* private data for callback */
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    GdkPixbuf *pixbuf;
    GdkPixbufAnimation *animation;
    GdkPixbufLoader *loader = NULL;
    GdkPixbufAnimationIter *it = NULL;
    gboolean loader_closed = FALSE;  /* remember if loader was already closed */
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
    GTimeVal time_val;
    GTimeVal start_time;
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
    sixel_frame_t *frame = NULL;
    int stride;
    unsigned char *p;
    int i;
    int depth;
    int anim_loop_count = (-1);  /* (-1): infinite, >=0: finite loop count */
    int delay_ms;

    (void) fuse_palette;
    (void) reqcolors;
    (void) bgcolor;

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

#if (! GLIB_CHECK_VERSION(2, 36, 0))
    g_type_init();
#endif
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
    g_get_current_time(&time_val);
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
    start_time = time_val;
    loader = gdk_pixbuf_loader_new();
    if (loader == NULL) {
        status = SIXEL_GDK_ERROR;
        goto end;
    }
    /* feed the whole blob and close so the animation metadata becomes available */
    if (! gdk_pixbuf_loader_write(loader, pchunk->buffer, pchunk->size, NULL)) {
        status = SIXEL_GDK_ERROR;
        goto end;
    }
    if (! gdk_pixbuf_loader_close(loader, NULL)) {
        status = SIXEL_GDK_ERROR;
        goto end;
    }
    loader_closed = TRUE;
    animation = gdk_pixbuf_loader_get_animation(loader);
    if (animation) {
        /* inspect animation object to determine built-in loop semantics */
        if (GDK_IS_PIXBUF_SIMPLE_ANIM(animation)) {
            anim_loop_count = gdk_pixbuf_simple_anim_get_loop(
                                 GDK_PIXBUF_SIMPLE_ANIM(animation)) ? (-1) : 1;
        } else {
            GParamSpec *loop_pspec = g_object_class_find_property(
                G_OBJECT_GET_CLASS(animation), "loop");
            if (loop_pspec == NULL) {
                loop_pspec = g_object_class_find_property(
                    G_OBJECT_GET_CLASS(animation), "loop-count");
            }
            if (loop_pspec) {
                GValue loop_value = G_VALUE_INIT;
                g_value_init(&loop_value, loop_pspec->value_type);
                g_object_get_property(G_OBJECT(animation),
                                      g_param_spec_get_name(loop_pspec),
                                      &loop_value);
                if (G_VALUE_HOLDS_BOOLEAN(&loop_value)) {
                    /* TRUE means "loop forever" for these properties */
                    anim_loop_count = g_value_get_boolean(&loop_value) ? (-1) : 1;
                } else if (G_VALUE_HOLDS_INT(&loop_value)) {
                    int loop_int = g_value_get_int(&loop_value);
                    /* GIF spec treats zero as infinite repetition */
                    anim_loop_count = (loop_int <= 0) ? (-1) : loop_int;
                } else if (G_VALUE_HOLDS_UINT(&loop_value)) {
                    guint loop_uint = g_value_get_uint(&loop_value);
                    if (loop_uint == 0U) {
                        anim_loop_count = (-1);
                    } else {
                        anim_loop_count = loop_uint > (guint)INT_MAX
                                            ? INT_MAX
                                            : (int)loop_uint;
                    }
                }
                g_value_unset(&loop_value);
            }
        }
    }
    if (!animation || fstatic || gdk_pixbuf_animation_is_static_image(animation)) {
        /* fall back to single frame decoding */
        pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
        if (pixbuf == NULL) {
            goto end;
        }
        frame->frame_no = 0;
        frame->width = gdk_pixbuf_get_width(pixbuf);
        frame->height = gdk_pixbuf_get_height(pixbuf);
        stride = gdk_pixbuf_get_rowstride(pixbuf);
        frame->pixels = sixel_allocator_malloc(pchunk->allocator, (size_t)(frame->height * stride));
        if (frame->pixels == NULL) {
            sixel_helper_set_additional_message(
                "load_with_gdkpixbuf: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (stride / frame->width == 4) {
            frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
            depth = 4;
        } else {
            frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
            depth = 3;
        }
        p = gdk_pixbuf_get_pixels(pixbuf);
        if (stride == frame->width * depth) {
            memcpy(frame->pixels, gdk_pixbuf_get_pixels(pixbuf),
                   (size_t)(frame->height * stride));
        } else {
            for (i = 0; i < frame->height; ++i) {
                memcpy(frame->pixels + frame->width * depth * i,
                       p + stride * i,
                       (size_t)(frame->width * depth));
            }
        }
        status = fn_load(frame, context);
        if (status != SIXEL_OK) {
            goto end;
        }
        /* scratch buffer no longer needed after callback */
        sixel_allocator_free(pchunk->allocator, frame->pixels);
        frame->pixels = NULL;
    } else {
        gboolean finished;

        /* reset iterator to the beginning of the timeline */
        time_val = start_time;
        frame->frame_no = 0;
        frame->loop_count = 0;

        it = gdk_pixbuf_animation_get_iter(animation, &time_val);
        if (it == NULL) {
            status = SIXEL_GDK_ERROR;
            goto end;
        }

        for (;;) {
            /* handle one logical loop of the animation */
            finished = FALSE;
            while (!gdk_pixbuf_animation_iter_on_currently_loading_frame(it)) {

                pixbuf = gdk_pixbuf_animation_iter_get_pixbuf(it);
                if (pixbuf == NULL) {
                    finished = TRUE;
                    break;
                }
                /* allocate a scratch copy of the current frame */
                frame->width = gdk_pixbuf_get_width(pixbuf);
                frame->height = gdk_pixbuf_get_height(pixbuf);
                stride = gdk_pixbuf_get_rowstride(pixbuf);
                frame->pixels = sixel_allocator_malloc(
                    pchunk->allocator,
                    (size_t)(frame->height * stride));
                if (frame->pixels == NULL) {
                    sixel_helper_set_additional_message(
                        "load_with_gdkpixbuf: sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto end;
                }
                if (gdk_pixbuf_get_has_alpha(pixbuf)) {
                    frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
                    depth = 4;
                } else {
                    frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
                    depth = 3;
                }
                p = gdk_pixbuf_get_pixels(pixbuf);
                if (stride == frame->width * depth) {
                    memcpy(frame->pixels, p,
                           (size_t)(frame->height * stride));
                } else {
                    for (i = 0; i < frame->height; ++i) {
                        memcpy(frame->pixels + frame->width * depth * i,
                               p + stride * i,
                               (size_t)(frame->width * depth));
                    }
                }
                delay_ms = gdk_pixbuf_animation_iter_get_delay_time(it);
                if (delay_ms < 0) {
                    delay_ms = 0;
                }
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
                /* advance the synthetic clock before asking gdk to move forward */
                g_time_val_add(&time_val, delay_ms * 1000);
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif  /* HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS */
                frame->delay = delay_ms / 10;
                frame->multiframe = 1;

                if (!gdk_pixbuf_animation_iter_advance(it, &time_val)) {
                    finished = TRUE;
                }
                status = fn_load(frame, context);
                if (status != SIXEL_OK) {
                    goto end;
                }
                /* release scratch pixels before decoding the next frame */
                sixel_allocator_free(pchunk->allocator, frame->pixels);
                frame->pixels = NULL;
                frame->frame_no++;

                if (finished) {
                    break;
                }
            }

            if (frame->frame_no == 0) {
                break;
            }

            /* finished processing one full loop */
            ++frame->loop_count;

            if (loop_control == SIXEL_LOOP_DISABLE || frame->frame_no == 1) {
                break;
            }
            if (loop_control == SIXEL_LOOP_AUTO) {
                /* obey header-provided loop count when AUTO */
                if (anim_loop_count >= 0 &&
                    frame->loop_count >= anim_loop_count) {
                    break;
                }
            } else if (loop_control != SIXEL_LOOP_FORCE &&
                       anim_loop_count > 0 &&
                       frame->loop_count >= anim_loop_count) {
                break;
            }

            /* restart iteration from the beginning for the next pass */
            g_object_unref(it);
            time_val = start_time;
            it = gdk_pixbuf_animation_get_iter(animation, &time_val);
            if (it == NULL) {
                status = SIXEL_GDK_ERROR;
                goto end;
            }
            /* next pass starts counting frames from zero again */
            frame->frame_no = 0;
        }
    }

    status = SIXEL_OK;

end:
    if (frame) {
        /* drop the reference we obtained from sixel_frame_new() */
        sixel_frame_unref(frame);
    }
    if (it) {
        g_object_unref(it);
    }
    if (loader) {
        if (!loader_closed) {
            /* ensure the incremental loader is finalized even on error paths */
            gdk_pixbuf_loader_close(loader, NULL);
        }
        g_object_unref(loader);
    }

    return status;

}
#endif  /* HAVE_GDK_PIXBUF2 */

#if HAVE_COREGRAPHICS
static SIXELSTATUS
load_with_coregraphics(
    sixel_chunk_t const       /* in */     *pchunk,
    int                       /* in */     fstatic,
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    sixel_load_image_function /* in */     fn_load,
    void                      /* in/out */ *context)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_frame_t *frame = NULL;
    CFDataRef data = NULL;
    CGImageSourceRef source = NULL;
    CGImageRef image = NULL;
    CGColorSpaceRef color_space = NULL;
    CGContextRef ctx = NULL;
    size_t stride;
    size_t frame_count;
    int anim_loop_count = (-1);
    CFDictionaryRef props = NULL;
    CFDictionaryRef anim_dict;
    CFNumberRef loop_num;
    CFDictionaryRef frame_props;
    CFDictionaryRef frame_anim_dict;
    CFNumberRef delay_num;
    double delay_sec;
    size_t i;

    (void) fuse_palette;
    (void) reqcolors;
    (void) bgcolor;

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    data = CFDataCreate(kCFAllocatorDefault,
                        pchunk->buffer,
                        (CFIndex)pchunk->size);
    if (! data) {
        status = SIXEL_FALSE;
        goto end;
    }

    source = CGImageSourceCreateWithData(data, NULL);
    if (! source) {
        status = SIXEL_FALSE;
        goto end;
    }

    frame_count = CGImageSourceGetCount(source);
    if (! frame_count) {
        status = SIXEL_FALSE;
        goto end;
    }
    if (fstatic) {
        frame_count = 1;
    }

    props = CGImageSourceCopyProperties(source, NULL);
    if (props) {
        anim_dict = (CFDictionaryRef)CFDictionaryGetValue(
            props, kCGImagePropertyGIFDictionary);
        if (anim_dict) {
            loop_num = (CFNumberRef)CFDictionaryGetValue(
                anim_dict, kCGImagePropertyGIFLoopCount);
            if (loop_num) {
                CFNumberGetValue(loop_num, kCFNumberIntType, &anim_loop_count);
            }
        }
        CFRelease(props);
    }

    color_space = CGColorSpaceCreateDeviceRGB();
    if (! color_space) {
        status = SIXEL_FALSE;
        goto end;
    }

    frame->loop_count = 0;

    for (;;) {
        frame->frame_no = 0;
        for (i = 0; i < frame_count; ++i) {
            delay_sec = 0.0;
            frame_props = CGImageSourceCopyPropertiesAtIndex(
                source, (CFIndex)i, NULL);
            if (frame_props) {
                frame_anim_dict = (CFDictionaryRef)CFDictionaryGetValue(
                    frame_props, kCGImagePropertyGIFDictionary);
                if (frame_anim_dict) {
                    delay_num = (CFNumberRef)CFDictionaryGetValue(
                        frame_anim_dict, kCGImagePropertyGIFUnclampedDelayTime);
                    if (! delay_num) {
                        delay_num = (CFNumberRef)CFDictionaryGetValue(
                            frame_anim_dict, kCGImagePropertyGIFDelayTime);
                    }
                    if (delay_num) {
                        CFNumberGetValue(delay_num,
                                         kCFNumberDoubleType,
                                         &delay_sec);
                    }
                }
#if defined(kCGImagePropertyPNGDictionary) && \
    defined(kCGImagePropertyAPNGUnclampedDelayTime) && \
    defined(kCGImagePropertyAPNGDelayTime)
                if (delay_sec <= 0.0) {
                    CFDictionaryRef png_frame = (CFDictionaryRef)CFDictionaryGetValue(
                        frame_props, kCGImagePropertyPNGDictionary);
                    if (png_frame) {
                        delay_num = (CFNumberRef)CFDictionaryGetValue(
                            png_frame, kCGImagePropertyAPNGUnclampedDelayTime);
                        if (! delay_num) {
                            delay_num = (CFNumberRef)CFDictionaryGetValue(
                                png_frame, kCGImagePropertyAPNGDelayTime);
                        }
                        if (delay_num) {
                            CFNumberGetValue(delay_num,
                                             kCFNumberDoubleType,
                                             &delay_sec);
                        }
                    }
                }
#endif
                CFRelease(frame_props);
            }
            if (delay_sec <= 0.0) {
                delay_sec = 0.1;
            }
            frame->delay = (int)(delay_sec * 100.0 + 0.5);
            if (frame->delay < 1) {
                frame->delay = 1;
            }

            image = CGImageSourceCreateImageAtIndex(source, (CFIndex)i, NULL);
            if (! image) {
                status = SIXEL_FALSE;
                goto end;
            }

            frame->width = (int)CGImageGetWidth(image);
            frame->height = (int)CGImageGetHeight(image);
            frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
            stride = (size_t)frame->width * 4;
            frame->pixels = sixel_allocator_malloc(
                pchunk->allocator, (size_t)(frame->height * stride));

            if (frame->pixels == NULL) {
                sixel_helper_set_additional_message(
                    "load_with_coregraphics: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                CGImageRelease(image);
                goto end;
            }

            ctx = CGBitmapContextCreate(frame->pixels,
                                        frame->width,
                                        frame->height,
                                        8,
                                        stride,
                                        color_space,
                                        kCGImageAlphaPremultipliedLast |
                                            kCGBitmapByteOrder32Big);
            if (!ctx) {
                CGImageRelease(image);
                goto end;
            }

            CGContextDrawImage(ctx,
                               CGRectMake(0, 0, frame->width, frame->height),
                               image);
            CGContextRelease(ctx);
            ctx = NULL;

            frame->multiframe = (frame_count > 1);
            status = fn_load(frame, context);
            sixel_allocator_free(pchunk->allocator, frame->pixels);
            frame->pixels = NULL;
            CGImageRelease(image);
            image = NULL;
            if (status != SIXEL_OK) {
                goto end;
            }
            ++frame->frame_no;
        }

        ++frame->loop_count;

        if (frame_count <= 1) {
            break;
        }
        if (loop_control == SIXEL_LOOP_DISABLE) {
            break;
        }
        if (loop_control == SIXEL_LOOP_AUTO) {
            if (anim_loop_count < 0) {
                break;
            }
            if (anim_loop_count > 0 && frame->loop_count >= anim_loop_count) {
                break;
            }
            continue;
        }
    }

    status = SIXEL_OK;

end:
    if (ctx) {
        CGContextRelease(ctx);
    }
    if (color_space) {
        CGColorSpaceRelease(color_space);
    }
    if (image) {
        CGImageRelease(image);
    }
    if (source) {
        CFRelease(source);
    }
    if (data) {
        CFRelease(data);
    }
    if (frame) {
        sixel_allocator_free(pchunk->allocator, frame->pixels);
        sixel_allocator_free(pchunk->allocator, frame->palette);
        sixel_allocator_free(pchunk->allocator, frame);
    }
    return status;
}
#endif  /* HAVE_COREGRAPHICS */

#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
static SIXELSTATUS
load_with_quicklook(
    sixel_chunk_t const       /* in */     *pchunk,
    int                       /* in */     fstatic,
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    sixel_load_image_function /* in */     fn_load,
    void                      /* in/out */ *context)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_frame_t *frame = NULL;
    CFStringRef path = NULL;
    CFURLRef url = NULL;
    CGImageRef image = NULL;
    CGColorSpaceRef color_space = NULL;
    CGContextRef ctx = NULL;
    CGRect bounds;
    size_t stride;
    unsigned char fill_color[3];
    CGFloat fill_r;
    CGFloat fill_g;
    CGFloat fill_b;
    CGFloat max_dimension;

    (void)fstatic;
    (void)fuse_palette;
    (void)reqcolors;
    (void)loop_control;

    if (pchunk == NULL || pchunk->source_path == NULL) {
        goto end;
    }

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    path = CFStringCreateWithCString(kCFAllocatorDefault,
                                     pchunk->source_path,
                                     kCFStringEncodingUTF8);
    if (path == NULL) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                        path,
                                        kCFURLPOSIXPathStyle,
                                        false);
    if (url == NULL) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    CGSize max_size;
    if (thumbnailer_size_hint > 0) {
        max_dimension = (CGFloat)thumbnailer_size_hint;
    } else {
        max_dimension = (CGFloat)SIXEL_THUMBNAILER_DEFAULT_SIZE;
    }
    max_size.width = max_dimension;
    max_size.height = max_dimension;
#if HAVE_QUICKLOOK_THUMBNAILING
    image = sixel_quicklook_thumbnail_create(url, max_size);
    if (image == NULL) {
# if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
# endif
        image = QLThumbnailImageCreate(kCFAllocatorDefault,
                                       url,
                                       max_size,
                                       NULL);
# if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma clang diagnostic pop
# endif
    }
#else
# if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
# endif
    image = QLThumbnailImageCreate(kCFAllocatorDefault,
                                   url,
                                   max_size,
                                   NULL);
# if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma clang diagnostic pop
# endif
#endif
    if (image == NULL) {
        status = SIXEL_RUNTIME_ERROR;
        sixel_helper_set_additional_message(
            "load_with_quicklook: CQLThumbnailImageCreate() failed.");
        goto end;
    }

    color_space = CGColorSpaceCreateDeviceRGB();
    if (color_space == NULL) {
        status = SIXEL_RUNTIME_ERROR;
        sixel_helper_set_additional_message(
            "load_with_quicklook: CGColorSpaceCreateDeviceRGB() failed.");
        goto end;
    }

    frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    frame->width = (int)CGImageGetWidth(image);
    frame->height = (int)CGImageGetHeight(image);
    if (frame->width <= 0 || frame->height <= 0) {
        status = SIXEL_RUNTIME_ERROR;
        sixel_helper_set_additional_message(
            "load_with_quicklook: invalid image size detected.");
        goto end;
    }

    stride = (size_t)frame->width * 4;
    frame->pixels =
        sixel_allocator_malloc(pchunk->allocator,
                               (size_t)frame->height * stride);
    if (frame->pixels == NULL) {
        sixel_helper_set_additional_message(
            "load_with_quicklook: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    if (bgcolor != NULL) {
        fill_color[0] = bgcolor[0];
        fill_color[1] = bgcolor[1];
        fill_color[2] = bgcolor[2];
    } else {
        fill_color[0] = 255;
        fill_color[1] = 255;
        fill_color[2] = 255;
    }

    ctx = CGBitmapContextCreate(frame->pixels,
                                frame->width,
                                frame->height,
                                8,
                                stride,
                                color_space,
                                kCGImageAlphaPremultipliedLast |
                                    kCGBitmapByteOrder32Big);
    if (ctx == NULL) {
        status = SIXEL_RUNTIME_ERROR;
        sixel_helper_set_additional_message(
            "load_with_quicklook: CGBitmapContextCreate() failed.");
        goto end;
    }

    bounds = CGRectMake(0,
                        0,
                        (CGFloat)frame->width,
                        (CGFloat)frame->height);
    fill_r = (CGFloat)fill_color[0] / 255.0f;
    fill_g = (CGFloat)fill_color[1] / 255.0f;
    fill_b = (CGFloat)fill_color[2] / 255.0f;
    CGContextSetRGBFillColor(ctx, fill_r, fill_g, fill_b, 1.0f);
    CGContextFillRect(ctx, bounds);
    CGContextDrawImage(ctx, bounds, image);
    CGContextFlush(ctx);

    /* Abort when Quick Look produced no visible pixels so other loaders run. */
    {
        size_t pixel_count;
        size_t index;
        unsigned char *pixel;
        int has_content;

        pixel_count = (size_t)frame->width * (size_t)frame->height;
        pixel = frame->pixels;
        has_content = 0;
        for (index = 0; index < pixel_count; ++index) {
            if (pixel[0] != fill_color[0] ||
                    pixel[1] != fill_color[1] ||
                    pixel[2] != fill_color[2] ||
                    pixel[3] != 0xff) {
                has_content = 1;
                break;
            }
            pixel += 4;
        }
        if (! has_content) {
            sixel_helper_set_additional_message(
                "load_with_quicklook: thumbnail contained no visible pixels.");
            status = SIXEL_BAD_INPUT;
            CGContextRelease(ctx);
            ctx = NULL;
            goto end;
        }
    }

    CGContextRelease(ctx);
    ctx = NULL;

    frame->delay = 0;
    frame->frame_no = 0;
    frame->loop_count = 1;
    frame->multiframe = 0;
    frame->transparent = (-1);

    status = sixel_frame_strip_alpha(frame, fill_color);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = fn_load(frame, context);
    sixel_allocator_free(pchunk->allocator, frame->pixels);
    frame->pixels = NULL;
    if (status != SIXEL_OK) {
        goto end;
    }

    status = SIXEL_OK;

end:
    if (ctx != NULL) {
        CGContextRelease(ctx);
    }
    if (color_space != NULL) {
        CGColorSpaceRelease(color_space);
    }
    if (image != NULL) {
        CGImageRelease(image);
    }
    if (url != NULL) {
        CFRelease(url);
    }
    if (path != NULL) {
        CFRelease(path);
    }
    if (frame != NULL) {
        sixel_allocator_free(pchunk->allocator, frame->pixels);
        sixel_allocator_free(pchunk->allocator, frame->palette);
        sixel_allocator_free(pchunk->allocator, frame);
    }

    return status;
}
#endif  /* HAVE_COREGRAPHICS && HAVE_QUICKLOOK */

#if HAVE_UNISTD_H && HAVE_SYS_WAIT_H && HAVE_FORK

#include <dirent.h>

#if !defined(_WIN32)
extern int mkstemp(char *);
#endif

#if !defined(_WIN32) && defined(HAVE__REALPATH) && !defined(HAVE_REALPATH)
static char *
thumbnailer_resolve_without_realpath(char const *path)
{
    char *cwd;
    char *resolved;
    size_t cwd_length;
    size_t path_length;
    int need_separator;

    cwd = NULL;
    resolved = NULL;
    cwd_length = 0;
    path_length = 0;
    need_separator = 0;

    if (path == NULL) {
        return NULL;
    }

    if (path[0] == '/') {
        path_length = strlen(path);
        resolved = malloc(path_length + 1);
        if (resolved == NULL) {
            return NULL;
        }
        memcpy(resolved, path, path_length + 1);

        return resolved;
    }

#if defined(PATH_MAX)
    cwd = malloc(PATH_MAX);
    if (cwd != NULL) {
        if (getcwd(cwd, PATH_MAX) != NULL) {
            cwd_length = strlen(cwd);
            path_length = strlen(path);
            need_separator = 0;
            if (cwd_length > 0 && cwd[cwd_length - 1] != '/') {
                need_separator = 1;
            }
            resolved = malloc(cwd_length + need_separator + path_length + 1);
            if (resolved != NULL) {
                memcpy(resolved, cwd, cwd_length);
                if (need_separator != 0) {
                    resolved[cwd_length] = '/';
                }
                memcpy(resolved + cwd_length + need_separator,
                       path,
                       path_length + 1);
            }
            free(cwd);
            if (resolved != NULL) {
                return resolved;
            }
        } else {
            free(cwd);
        }
    }
#endif  /* PATH_MAX */

    path_length = strlen(path);
    resolved = malloc(path_length + 1);
    if (resolved == NULL) {
        return NULL;
    }
    memcpy(resolved, path, path_length + 1);

    return resolved;
}
#endif  /* !defined(_WIN32) && defined(HAVE__REALPATH) && !defined(HAVE_REALPATH) */

/*
 * thumbnailer_resolve_path
 *
 * Resolve the supplied path to an absolute canonical path when possible.
 *
 * Arguments:
 *     path - original filesystem path.
 * Returns:
 *     Newly allocated canonical path or NULL on failure.
 */
static char *
thumbnailer_resolve_path(char const *path)
{
    char *resolved;

    resolved = NULL;

    if (path == NULL) {
        return NULL;
    }

#if defined(_WIN32)
    resolved = _fullpath(NULL, path, 0);
#elif defined(HAVE__REALPATH)
    resolved = _realpath(path, NULL);
#elif defined(HAVE_REALPATH)
    resolved = realpath(path, NULL);
#else
    resolved = thumbnailer_resolve_without_realpath(path);
#endif

    return resolved;
}

struct thumbnailer_string_list {
    char **items;
    size_t length;
    size_t capacity;
};

struct thumbnailer_entry {
    char *exec_line;
    char *tryexec;
    struct thumbnailer_string_list *mime_types;
};

/*
 * thumbnailer_strdup
 *
 * Duplicate a string with malloc so thumbnail helpers own their copies.
 *
 * Arguments:
 *     src - zero-terminated string to copy; may be NULL.
 * Returns:
 *     Newly allocated duplicate or NULL on failure/NULL input.
 */
static char *
thumbnailer_strdup(char const *src)
{
    char *copy;
    size_t length;

    copy = NULL;
    length = 0;

    if (src == NULL) {
        return NULL;
    }

    length = strlen(src);
    copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, src, length + 1);

    return copy;
}

/*
 * thumbnailer_string_list_new
 *
 * Allocate an empty expandable string list used throughout the loader.
 *
 * Arguments:
 *     None.
 * Returns:
 *     Newly allocated list instance or NULL on failure.
 */
static struct thumbnailer_string_list *
thumbnailer_string_list_new(void)
{
    struct thumbnailer_string_list *list;

    list = malloc(sizeof(*list));
    if (list == NULL) {
        return NULL;
    }

    list->items = NULL;
    list->length = 0;
    list->capacity = 0;

    return list;
}

/*
 * thumbnailer_string_list_free
 *
 * Release every string stored in the list and free the container itself.
 *
 * Arguments:
 *     list - list instance produced by thumbnailer_string_list_new().
 */
static void
thumbnailer_string_list_free(struct thumbnailer_string_list *list)
{
    size_t index;

    index = 0;

    if (list == NULL) {
        return;
    }

    if (list->items != NULL) {
        for (index = 0; index < list->length; ++index) {
            free(list->items[index]);
            list->items[index] = NULL;
        }
        free(list->items);
        list->items = NULL;
    }

    free(list);
}

/*
 * thumbnailer_string_list_append
 *
 * Append a copy of the supplied string to the dynamic list.
 *
 * Arguments:
 *     list  - destination list.
 *     value - string to duplicate and append.
 * Returns:
 *     1 on success, 0 on allocation failure or invalid input.
 */
static int
thumbnailer_string_list_append(struct thumbnailer_string_list *list,
                               char const *value)
{
    size_t new_capacity;
    char **new_items;
    char *copy;

    new_capacity = 0;
    new_items = NULL;
    copy = NULL;

    if (list == NULL || value == NULL) {
        return 0;
    }

    copy = thumbnailer_strdup(value);
    if (copy == NULL) {
        return 0;
    }

    if (list->length == list->capacity) {
        new_capacity = (list->capacity == 0) ? 4 : list->capacity * 2;
        new_items = realloc(list->items,
                            new_capacity * sizeof(*list->items));
        if (new_items == NULL) {
            free(copy);
            return 0;
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }

    list->items[list->length] = copy;
    list->length += 1;

    return 1;
}

/*
 * thumbnailer_entry_init
 *
 * Prepare a thumbnailer_entry structure for population.
 *
 * Arguments:
 *     entry - caller-provided structure to initialize.
 */
static void
thumbnailer_entry_init(struct thumbnailer_entry *entry)
{
    if (entry == NULL) {
        return;
    }

    entry->exec_line = NULL;
    entry->tryexec = NULL;
    entry->mime_types = NULL;
}

/*
 * thumbnailer_entry_clear
 *
 * Release every heap allocation associated with a thumbnailer_entry.
 *
 * Arguments:
 *     entry - structure previously initialized with thumbnailer_entry_init().
 */
static void
thumbnailer_entry_clear(struct thumbnailer_entry *entry)
{
    if (entry == NULL) {
        return;
    }

    free(entry->exec_line);
    entry->exec_line = NULL;
    free(entry->tryexec);
    entry->tryexec = NULL;
    thumbnailer_string_list_free(entry->mime_types);
    entry->mime_types = NULL;
}

/*
 * thumbnailer_join_paths
 *
 * Concatenate two path fragments inserting a slash when required.
 *
 * Arguments:
 *     left  - directory prefix.
 *     right - trailing component.
 * Returns:
 *     Newly allocated combined path or NULL on failure.
 */
static char *
thumbnailer_join_paths(char const *left, char const *right)
{
    size_t left_length;
    size_t right_length;
    int need_separator;
    char *combined;

    left_length = 0;
    right_length = 0;
    need_separator = 0;
    combined = NULL;

    if (left == NULL || right == NULL) {
        return NULL;
    }

    left_length = strlen(left);
    right_length = strlen(right);
    need_separator = 0;

    if (left_length > 0 && right_length > 0 &&
            left[left_length - 1] != '/' && right[0] != '/') {
        need_separator = 1;
    }

    combined = malloc(left_length + right_length + need_separator + 1);
    if (combined == NULL) {
        return NULL;
    }

    memcpy(combined, left, left_length);
    if (need_separator) {
        combined[left_length] = '/';
        memcpy(combined + left_length + 1, right, right_length);
        combined[left_length + right_length + 1] = '\0';
    } else {
        memcpy(combined + left_length, right, right_length);
        combined[left_length + right_length] = '\0';
    }

    return combined;
}

/*
 * thumbnailer_collect_directories
 *
 * Enumerate directories that may contain FreeDesktop thumbnailer
 * definitions according to the XDG specification.
 *
 * GNOME thumbnailers follow the XDG data directory contract:
 *
 *     +------------------+      +---------------------------+
 *     | HOME/.local/share| ---> | HOME/.local/share/        |
 *     |                  |      |    thumbnailers/(*.thumbnailer)
 *     +------------------+      +---------------------------+
 *
 *     +------------------+      +---------------------------+
 *     | XDG_DATA_DIRS    | ---> | <dir>/thumbnailers/(*.thumbnailer)
 *     +------------------+      +---------------------------+
 *
 * The helper below expands both sources so that the caller can iterate
 * through every known definition in order of precedence.
 *
 * Arguments:
 *     None.
 * Returns:
 *     Newly allocated list of directory paths or NULL on failure.
 */
static struct thumbnailer_string_list *
thumbnailer_collect_directories(void)
{
    struct thumbnailer_string_list *dirs;
    char const *xdg_data_dirs;
    char const *home_dir;
    char const *default_dirs;
    char *candidate;
    char *local_share;
    char *dirs_copy;
    char *token;

    dirs = NULL;
    xdg_data_dirs = NULL;
    home_dir = NULL;
    default_dirs = NULL;
    candidate = NULL;
    local_share = NULL;
    dirs_copy = NULL;
    token = NULL;

    dirs = thumbnailer_string_list_new();
    if (dirs == NULL) {
        return NULL;
    }

    home_dir = getenv("HOME");
    loader_trace_message(
        "thumbnailer_collect_directories: HOME=%s",
        (home_dir != NULL && home_dir[0] != '\0') ? home_dir : "(unset)");
    if (home_dir != NULL && home_dir[0] != '\0') {
        local_share = thumbnailer_join_paths(home_dir,
                                             ".local/share");
        if (local_share != NULL) {
            candidate = thumbnailer_join_paths(local_share,
                                               "thumbnailers");
            if (candidate != NULL) {
                if (!thumbnailer_string_list_append(dirs, candidate)) {
                    free(candidate);
                    free(local_share);
                    thumbnailer_string_list_free(dirs);
                    return NULL;
                }
                loader_trace_message(
                    "thumbnailer_collect_directories: added %s",
                    candidate);
                free(candidate);
                candidate = NULL;
            }
            free(local_share);
            local_share = NULL;
        }
    }

    xdg_data_dirs = getenv("XDG_DATA_DIRS");
    if (xdg_data_dirs == NULL || xdg_data_dirs[0] == '\0') {
        default_dirs = "/usr/local/share:/usr/share";
        xdg_data_dirs = default_dirs;
    }
    loader_trace_message(
        "thumbnailer_collect_directories: XDG_DATA_DIRS=%s",
        xdg_data_dirs);

    dirs_copy = thumbnailer_strdup(xdg_data_dirs);
    if (dirs_copy == NULL) {
        thumbnailer_string_list_free(dirs);
        return NULL;
    }
    token = strtok(dirs_copy, ":");
    while (token != NULL) {
        candidate = thumbnailer_join_paths(token, "thumbnailers");
        if (candidate != NULL) {
            if (!thumbnailer_string_list_append(dirs, candidate)) {
                free(candidate);
                free(dirs_copy);
                thumbnailer_string_list_free(dirs);
                return NULL;
            }
            loader_trace_message(
                "thumbnailer_collect_directories: added %s",
                candidate);
            free(candidate);
            candidate = NULL;
        }
        token = strtok(NULL, ":");
    }
    free(dirs_copy);
    dirs_copy = NULL;

    return dirs;
}

/*
 * thumbnailer_trim_right
 *
 * Remove trailing whitespace in place from a mutable string.
 *
 * Arguments:
 *     text - string to trim; must be writable and zero-terminated.
 */
static void
thumbnailer_trim_right(char *text)
{
    size_t length;

    length = 0;

    if (text == NULL) {
        return;
    }

    length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1]) != 0) {
        text[length - 1] = '\0';
        length -= 1;
    }
}

/*
 * thumbnailer_trim_left
 *
 * Skip leading whitespace so parsers can focus on significant tokens.
 *
 * Arguments:
 *     text - string to inspect; may be NULL.
 * Returns:
 *     Pointer to first non-space character or NULL when input is NULL.
 */
static char *
thumbnailer_trim_left(char *text)
{
    if (text == NULL) {
        return NULL;
    }

    while (*text != '\0' && isspace((unsigned char)*text) != 0) {
        text += 1;
    }

    return text;
}

/*
 * thumbnailer_parse_file
 *
 * Populate a thumbnailer_entry by parsing a .thumbnailer ini file.
 *
 * Arguments:
 *     path  - filesystem path to the ini file.
 *     entry - output structure initialized with thumbnailer_entry_init().
 * Returns:
 *     1 on success, 0 on parse error or allocation failure.
 */
static int
thumbnailer_parse_file(char const *path, struct thumbnailer_entry *entry)
{
    FILE *fp;
    char line[1024];
    int in_group;
    char *trimmed;
    char *key_end;
    char *value;
    char *token_start;
    char *token_end;
    struct thumbnailer_string_list *mime_types;
    size_t index;

    fp = NULL;
    in_group = 0;
    trimmed = NULL;
    key_end = NULL;
    value = NULL;
    token_start = NULL;
    token_end = NULL;
    mime_types = NULL;
    index = 0;

    if (path == NULL || entry == NULL) {
        return 0;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        return 0;
    }

    mime_types = thumbnailer_string_list_new();
    if (mime_types == NULL) {
        fclose(fp);
        fp = NULL;
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        trimmed = thumbnailer_trim_left(line);
        thumbnailer_trim_right(trimmed);
        if (trimmed[0] == '\0' || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }
        if (trimmed[0] == '[') {
            key_end = strchr(trimmed, ']');
            if (key_end != NULL) {
                *key_end = '\0';
                if (strcmp(trimmed + 1, "Thumbnailer Entry") == 0) {
                    in_group = 1;
                } else {
                    in_group = 0;
                }
            }
            continue;
        }
        if (!in_group) {
            continue;
        }
        key_end = strchr(trimmed, '=');
        if (key_end == NULL) {
            continue;
        }
        *key_end = '\0';
        value = thumbnailer_trim_left(key_end + 1);
        thumbnailer_trim_right(trimmed);
        thumbnailer_trim_right(value);
        if (strcmp(trimmed, "Exec") == 0) {
            free(entry->exec_line);
            entry->exec_line = thumbnailer_strdup(value);
            if (entry->exec_line == NULL) {
                fclose(fp);
                fp = NULL;
                thumbnailer_string_list_free(mime_types);
                mime_types = NULL;
                return 0;
            }
        } else if (strcmp(trimmed, "TryExec") == 0) {
            free(entry->tryexec);
            entry->tryexec = thumbnailer_strdup(value);
            if (entry->tryexec == NULL) {
                fclose(fp);
                fp = NULL;
                thumbnailer_string_list_free(mime_types);
                mime_types = NULL;
                return 0;
            }
        } else if (strcmp(trimmed, "MimeType") == 0) {
            for (index = 0; index < mime_types->length; ++index) {
                free(mime_types->items[index]);
                mime_types->items[index] = NULL;
            }
            mime_types->length = 0;
            token_start = value;
            while (token_start != NULL && token_start[0] != '\0') {
                token_end = strchr(token_start, ';');
                if (token_end != NULL) {
                    *token_end = '\0';
                }
                token_start = thumbnailer_trim_left(token_start);
                thumbnailer_trim_right(token_start);
                if (token_start[0] != '\0') {
                    if (!thumbnailer_string_list_append(mime_types,
                                                       token_start)) {
                        fclose(fp);
                        fp = NULL;
                        thumbnailer_string_list_free(mime_types);
                        mime_types = NULL;
                        return 0;
                    }
                }
                if (token_end == NULL) {
                    break;
                }
                token_start = token_end + 1;
            }
        }
    }

    fclose(fp);
    fp = NULL;

    thumbnailer_string_list_free(entry->mime_types);
    entry->mime_types = mime_types;

    return 1;
}

/*
 * thumbnailer_has_tryexec
 *
 * Confirm that the optional TryExec binary exists and is executable.
 *
 * Arguments:
 *     tryexec - value from the .thumbnailer file; may be NULL.
 * Returns:
 *     1 when executable, 0 otherwise.
 */
static int
thumbnailer_has_tryexec(char const *tryexec)
{
    char const *path_variable;
    char const *start;
    char const *end;
    size_t length;
    char *candidate;
    int executable;

    path_variable = NULL;
    start = NULL;
    end = NULL;
    length = 0;
    candidate = NULL;
    executable = 0;

    if (tryexec == NULL || tryexec[0] == '\0') {
        return 1;
    }

    if (strchr(tryexec, '/') != NULL) {
        if (access(tryexec, X_OK) == 0) {
            return 1;
        }
        return 0;
    }

    path_variable = getenv("PATH");
    if (path_variable == NULL) {
        return 0;
    }

    start = path_variable;
    while (*start != '\0') {
        end = strchr(start, ':');
        if (end == NULL) {
            end = start + strlen(start);
        }
        length = (size_t)(end - start);
        candidate = malloc(length + strlen(tryexec) + 2);
        if (candidate == NULL) {
            return 0;
        }
        memcpy(candidate, start, length);
        candidate[length] = '/';
        strcpy(candidate + length + 1, tryexec);
        if (access(candidate, X_OK) == 0) {
            executable = 1;
            free(candidate);
            candidate = NULL;
            break;
        }
        free(candidate);
        candidate = NULL;
        if (*end == '\0') {
            break;
        }
        start = end + 1;
    }

    return executable;
}

/*
 * thumbnailer_mime_matches
 *
 * Test whether a thumbnailer MIME pattern matches the probed MIME type.
 *
 * Arguments:
 *     pattern   - literal MIME pattern or prefix ending with "slash-asterisk".
 *     mime_type - MIME value obtained from file --mime-type.
 * Returns:
 *     1 when the pattern applies, 0 otherwise.
 */
static int
thumbnailer_mime_matches(char const *pattern, char const *mime_type)
{
    size_t length;

    length = 0;

    if (pattern == NULL || mime_type == NULL) {
        return 0;
    }

    if (strcmp(pattern, mime_type) == 0) {
        return 1;
    }

    length = strlen(pattern);
    if (length >= 2 && pattern[length - 1] == '*' &&
            pattern[length - 2] == '/') {
        return strncmp(pattern, mime_type, length - 1) == 0;
    }

    return 0;
}

/*
 * thumbnailer_supports_mime
 *
 * Iterate over MIME patterns advertised by a thumbnailer entry.
 *
 * Arguments:
 *     entry     - parsed thumbnailer entry with mime_types list.
 *     mime_type - MIME type string to match.
 * Returns:
 *     1 when a match is found, 0 otherwise.
 */
static int
thumbnailer_supports_mime(struct thumbnailer_entry *entry,
                          char const *mime_type)
{
    size_t index;

    index = 0;

    if (entry == NULL || entry->mime_types == NULL) {
        return 0;
    }

    if (mime_type == NULL) {
        return 0;
    }

    for (index = 0; index < entry->mime_types->length; ++index) {
        if (thumbnailer_mime_matches(entry->mime_types->items[index],
                                     mime_type)) {
            return 1;
        }
    }

    return 0;
}

/*
 * thumbnailer_shell_quote
 *
 * Produce a single-quoted variant of an argument for readable logging.
 *
 * Arguments:
 *     text - unquoted argument.
 * Returns:
 *     Newly allocated quoted string or NULL on allocation failure.
 */
static char *
thumbnailer_shell_quote(char const *text)
{
    size_t index;
    size_t length;
    size_t needed;
    char *quoted;
    size_t position;

    index = 0;
    length = 0;
    needed = 0;
    quoted = NULL;
    position = 0;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    needed = 2;
    for (index = 0; index < length; ++index) {
        if (text[index] == '\'') {
            needed += 4;
        } else {
            needed += 1;
        }
    }

    quoted = malloc(needed + 1);
    if (quoted == NULL) {
        return NULL;
    }

    quoted[position++] = '\'';
    for (index = 0; index < length; ++index) {
        if (text[index] == '\'') {
            quoted[position++] = '\'';
            quoted[position++] = '\\';
            quoted[position++] = '\'';
            quoted[position++] = '\'';
        } else {
            quoted[position++] = text[index];
        }
    }
    quoted[position++] = '\'';
    quoted[position] = '\0';

    return quoted;
}

struct thumbnailer_builder {
    char *buffer;
    size_t length;
    size_t capacity;
};

/*
 * thumbnailer_builder_reserve
 *
 * Grow the builder buffer so future appends fit without overflow.
 *
 * Arguments:
 *     builder    - mutable builder instance.
 *     additional - number of bytes that must fit excluding terminator.
 * Returns:
 *     1 on success, 0 on allocation failure.
 */
static int
thumbnailer_builder_reserve(struct thumbnailer_builder *builder,
                            size_t additional)
{
    size_t new_capacity;
    char *new_buffer;

    new_capacity = 0;
    new_buffer = NULL;

    if (builder->length + additional + 1 <= builder->capacity) {
        return 1;
    }

    new_capacity = (builder->capacity == 0) ? 64 : builder->capacity;
    while (new_capacity < builder->length + additional + 1) {
        new_capacity *= 2;
    }

    new_buffer = realloc(builder->buffer, new_capacity);
    if (new_buffer == NULL) {
        return 0;
    }

    builder->buffer = new_buffer;
    builder->capacity = new_capacity;

    return 1;
}

/*
 * thumbnailer_builder_append_char
 *
 * Append a single character to the builder.
 *
 * Arguments:
 *     builder - mutable builder instance.
 *     ch      - character to append.
 * Returns:
 *     1 on success, 0 on allocation failure.
 */
static int
thumbnailer_builder_append_char(struct thumbnailer_builder *builder,
                                char ch)
{
    if (!thumbnailer_builder_reserve(builder, 1)) {
        return 0;
    }

    builder->buffer[builder->length] = ch;
    builder->length += 1;
    builder->buffer[builder->length] = '\0';

    return 1;
}

/*
 * thumbnailer_builder_append
 *
 * Append a string of known length to the builder buffer.
 *
 * Arguments:
 *     builder - mutable builder instance.
 *     text    - zero-terminated string to append.
 * Returns:
 *     1 on success, 0 on allocation failure or NULL input.
 */
static int
thumbnailer_builder_append(struct thumbnailer_builder *builder,
                           char const *text)
{
    size_t length;

    length = 0;

    if (text == NULL) {
        return 1;
    }

    length = strlen(text);
    if (!thumbnailer_builder_reserve(builder, length)) {
        return 0;
    }

    memcpy(builder->buffer + builder->length, text, length);
    builder->length += length;
    builder->buffer[builder->length] = '\0';

    return 1;
}

/*
 * thumbnailer_builder_clear
 *
 * Reset builder length to zero while retaining allocated storage.
 *
 * Arguments:
 *     builder - builder to reset.
 */
static void
thumbnailer_builder_clear(struct thumbnailer_builder *builder)
{
    if (builder->buffer != NULL) {
        builder->buffer[0] = '\0';
    }
    builder->length = 0;
}

/*
 * thumbnailer_command owns the argv array that will be passed to the
 * thumbnailer helper.  The display field keeps a human readable command line
 * for verbose logging without recomputing the shell quoted form.
 */
struct thumbnailer_command {
    char **argv;
    size_t argc;
    char *display;
};

/*
 * thumbnailer_command_free
 *
 * Release argv entries, the array itself, and the formatted display copy.
 *
 * Arguments:
 *     command - structure created by thumbnailer_build_command().
 */
static void
thumbnailer_command_free(struct thumbnailer_command *command)
{
    size_t index;

    if (command == NULL) {
        return;
    }

    if (command->argv != NULL) {
        for (index = 0; index < command->argc; ++index) {
            free(command->argv[index]);
            command->argv[index] = NULL;
        }
        free(command->argv);
        command->argv = NULL;
    }

    free(command->display);
    command->display = NULL;

    free(command);
}

/*
 * thumbnailer_command_format
 *
 * Join argv entries into a human-readable command line for logging.
 *
 * Arguments:
 *     argv - array of argument strings.
 *     argc - number of entries stored in argv.
 * Returns:
 *     Newly allocated formatted string or NULL on allocation failure.
 */
static char *
thumbnailer_command_format(char **argv, size_t argc)
{
    struct thumbnailer_builder builder;
    char *quoted;
    size_t index;

    builder.buffer = NULL;
    builder.length = 0;
    builder.capacity = 0;
    quoted = NULL;

    for (index = 0; index < argc; ++index) {
        if (index > 0) {
            if (!thumbnailer_builder_append_char(&builder, ' ')) {
                free(builder.buffer);
                builder.buffer = NULL;
                return NULL;
            }
        }
        quoted = thumbnailer_shell_quote(argv[index]);
        if (quoted == NULL) {
            free(builder.buffer);
            builder.buffer = NULL;
            return NULL;
        }
        if (!thumbnailer_builder_append(&builder, quoted)) {
            free(quoted);
            quoted = NULL;
            free(builder.buffer);
            builder.buffer = NULL;
            return NULL;
        }
        free(quoted);
        quoted = NULL;
    }

    return builder.buffer;
}

/*
 * thumbnailer_build_command
 *
 * Expand a .thumbnailer Exec template into an argv array that honours
 * FreeDesktop substitution rules.
 *
 * Arguments:
 *     template_command - Exec line containing % tokens.
 *     input_path       - filesystem path to the source document.
 *     input_uri        - URI representation for %u expansions.
 *     output_path      - PNG destination path for %o expansions.
 *     size             - numeric size hint passed to %s tokens.
 *     mime_type        - MIME value for %m replacements.
 * Returns:
 *     Newly allocated command or NULL on parse/allocation failure.
 */
static struct thumbnailer_command *
thumbnailer_build_command(char const *template_command,
                          char const *input_path,
                          char const *input_uri,
                          char const *output_path,
                          int size,
                          char const *mime_type)
{
    struct thumbnailer_builder builder;
    struct thumbnailer_string_list *tokens;
    struct thumbnailer_command *command;
    char const *ptr;
    char size_text[16];
    int in_single_quote;
    int in_double_quote;
    int escape_next;
    char const *replacement;
    size_t index;

    builder.buffer = NULL;
    builder.length = 0;
    builder.capacity = 0;
    tokens = NULL;
    command = NULL;
    ptr = template_command;
    size_text[0] = '\0';
    in_single_quote = 0;
    in_double_quote = 0;
    escape_next = 0;
    replacement = NULL;
    index = 0;

    if (template_command == NULL) {
        return NULL;
    }

    tokens = thumbnailer_string_list_new();
    if (tokens == NULL) {
        return NULL;
    }

    if (size > 0) {
        if (!thumbnailer_safe_format(size_text,
                                     sizeof(size_text),
                                     "%d",
                                     size)) {
            goto error;
        }
    }

    while (ptr != NULL && ptr[0] != '\0') {
        if (!in_single_quote && !in_double_quote && escape_next == 0 &&
                (ptr[0] == ' ' || ptr[0] == '\t')) {
            if (builder.length > 0) {
                if (!thumbnailer_string_list_append(tokens,
                                                    builder.buffer)) {
                    goto error;
                }
                thumbnailer_builder_clear(&builder);
            }
            ptr += 1;
            continue;
        }
        if (!in_single_quote && escape_next == 0 && ptr[0] == '\\') {
            escape_next = 1;
            ptr += 1;
            continue;
        }
        if (!in_double_quote && escape_next == 0 && ptr[0] == '\'') {
            in_single_quote = !in_single_quote;
            ptr += 1;
            continue;
        }
        if (!in_single_quote && escape_next == 0 && ptr[0] == '"') {
            in_double_quote = !in_double_quote;
            ptr += 1;
            continue;
        }
        if (escape_next != 0) {
            if (!thumbnailer_builder_append_char(&builder, ptr[0])) {
                goto error;
            }
            escape_next = 0;
            ptr += 1;
            continue;
        }
        if (ptr[0] == '%' && ptr[1] != '\0') {
            replacement = NULL;
            ptr += 1;
            switch (ptr[0]) {
            case '%':
                if (!thumbnailer_builder_append_char(&builder, '%')) {
                    goto error;
                }
                break;
            case 'i':
            case 'I':
                replacement = input_path;
                break;
            case 'u':
            case 'U':
                replacement = input_uri;
                break;
            case 'o':
            case 'O':
                replacement = output_path;
                break;
            case 's':
            case 'S':
                replacement = size_text;
                break;
            case 'm':
            case 'M':
                replacement = mime_type;
                break;
            default:
                if (!thumbnailer_builder_append_char(&builder, '%') ||
                        !thumbnailer_builder_append_char(&builder,
                                                         ptr[0])) {
                    goto error;
                }
                break;
            }
            if (replacement != NULL) {
                if (!thumbnailer_builder_append(&builder, replacement)) {
                    goto error;
                }
            }
            ptr += 1;
            continue;
        }
        if (!thumbnailer_builder_append_char(&builder, ptr[0])) {
            goto error;
        }
        ptr += 1;
    }

    if (builder.length > 0) {
        if (!thumbnailer_string_list_append(tokens, builder.buffer)) {
            goto error;
        }
    }

    command = malloc(sizeof(*command));
    if (command == NULL) {
        goto error;
    }

    command->argc = tokens->length;
    command->argv = NULL;
    command->display = NULL;

    if (tokens->length == 0) {
        goto error;
    }

    command->argv = malloc(sizeof(char *) * (tokens->length + 1));
    if (command->argv == NULL) {
        goto error;
    }

    for (index = 0; index < tokens->length; ++index) {
        command->argv[index] = thumbnailer_strdup(tokens->items[index]);
        if (command->argv[index] == NULL) {
            goto error;
        }
    }
    command->argv[tokens->length] = NULL;

    command->display = thumbnailer_command_format(command->argv,
                                                  command->argc);
    if (command->display == NULL) {
        goto error;
    }

    thumbnailer_string_list_free(tokens);
    tokens = NULL;
    if (builder.buffer != NULL) {
        free(builder.buffer);
        builder.buffer = NULL;
    }

    return command;

error:
    if (tokens != NULL) {
        thumbnailer_string_list_free(tokens);
        tokens = NULL;
    }
    if (builder.buffer != NULL) {
        free(builder.buffer);
        builder.buffer = NULL;
    }
    if (command != NULL) {
        thumbnailer_command_free(command);
        command = NULL;
    }

    return NULL;
}

/*
 * thumbnailer_is_evince_thumbnailer
 *
 * Detect whether the selected thumbnailer maps to evince-thumbnailer so
 * the stdout redirection workaround can be applied.
 *
 * Arguments:
 *     exec_line - Exec string parsed from the .thumbnailer file.
 *     tryexec   - optional TryExec value for additional matching.
 * Returns:
 *     1 when evince-thumbnailer is referenced, 0 otherwise.
 */
static int
thumbnailer_is_evince_thumbnailer(char const *exec_line,
                                  char const *tryexec)
{
    char const *needle;
    char const *basename;

    needle = "evince-thumbnailer";
    basename = NULL;

    if (exec_line != NULL && strstr(exec_line, needle) != NULL) {
        return 1;
    }

    if (tryexec != NULL) {
        basename = strrchr(tryexec, '/');
        if (basename != NULL) {
            basename += 1;
        } else {
            basename = tryexec;
        }
        if (strcmp(basename, needle) == 0) {
            return 1;
        }
        if (strstr(tryexec, needle) != NULL) {
            return 1;
        }
    }

    return 0;
}

/*
 * thumbnailer_build_evince_command
 *
 * Construct an argv sequence that streams evince-thumbnailer output to
 * stdout so downstream code can capture the PNG safely.
 *
 * Arguments:
 *     input_path - source document path.
 *     size       - numeric size hint forwarded to the -s option.
 * Returns:
 *     Newly allocated command or NULL on allocation failure.
 */
static struct thumbnailer_command *
thumbnailer_build_evince_command(char const *input_path,
                                 int size)
{
    struct thumbnailer_command *command;
    char size_text[16];
    size_t index;

    command = NULL;
    index = 0;

    if (input_path == NULL) {
        return NULL;
    }

    command = malloc(sizeof(*command));
    if (command == NULL) {
        return NULL;
    }

    command->argc = 5;
    command->argv = malloc(sizeof(char *) * (command->argc + 1));
    if (command->argv == NULL) {
        thumbnailer_command_free(command);
        return NULL;
    }

    for (index = 0; index < command->argc + 1; ++index) {
        command->argv[index] = NULL;
    }

    if (!thumbnailer_safe_format(size_text,
                                 sizeof(size_text),
                                 "%d",
                                 size)) {
        size_text[0] = '\0';
    }

    command->argv[0] = thumbnailer_strdup("evince-thumbnailer");
    command->argv[1] = thumbnailer_strdup("-s");
    command->argv[2] = thumbnailer_strdup(size_text);
    command->argv[3] = thumbnailer_strdup(input_path);
    command->argv[4] = thumbnailer_strdup("/dev/stdout");
    command->argv[5] = NULL;

    for (index = 0; index < command->argc; ++index) {
        if (command->argv[index] == NULL) {
            thumbnailer_command_free(command);
            return NULL;
        }
    }

    command->display = thumbnailer_command_format(command->argv,
                                                  command->argc);
    if (command->display == NULL) {
        thumbnailer_command_free(command);
        return NULL;
    }

    return command;
}

/*
 * thumbnailer_build_file_uri
 *
 * Convert a filesystem path into a percent-encoded file:// URI.
 *
 * Arguments:
 *     path - filesystem path; may be relative but will be resolved.
 * Returns:
 *     Newly allocated URI string or NULL on error.
 */
static char *
thumbnailer_build_file_uri(char const *path)
{
    char *resolved;
    size_t index;
    size_t length;
    size_t needed;
    char *uri;
    size_t position;
    char const hex[] = "0123456789ABCDEF";

    resolved = NULL;
    index = 0;
    length = 0;
    needed = 0;
    uri = NULL;
    position = 0;

    if (path == NULL) {
        return NULL;
    }

    resolved = thumbnailer_resolve_path(path);
    if (resolved == NULL) {
        return NULL;
    }

    length = strlen(resolved);
    needed = 7;
    for (index = 0; index < length; ++index) {
        unsigned char ch;

        ch = (unsigned char)resolved[index];
        if (isalnum(ch) || ch == '-' || ch == '_' ||
                ch == '.' || ch == '~' || ch == '/') {
            needed += 1;
        } else {
            needed += 3;
        }
    }

    uri = malloc(needed + 1);
    if (uri == NULL) {
        free(resolved);
        resolved = NULL;
        return NULL;
    }

    memcpy(uri, "file://", 7);
    position = 7;
    for (index = 0; index < length; ++index) {
        unsigned char ch;

        ch = (unsigned char)resolved[index];
        if (isalnum(ch) || ch == '-' || ch == '_' ||
                ch == '.' || ch == '~' || ch == '/') {
            uri[position++] = (char)ch;
        } else {
            uri[position++] = '%';
            uri[position++] = hex[(ch >> 4) & 0xF];
            uri[position++] = hex[ch & 0xF];
        }
    }
    uri[position] = '\0';

    free(resolved);
    resolved = NULL;

    return uri;
}

/*
 * thumbnailer_guess_content_type
 *
 * Invoke the file(1) utility to obtain a MIME type for the input path.
 *
 * Arguments:
 *     path - filesystem path passed to file --mime-type.
 * Returns:
 *     Newly allocated MIME type string or NULL on failure.
 */
static char *
thumbnailer_guess_content_type(char const *path)
{
    int pipefd[2];
    pid_t pid;
    ssize_t bytes_read;
    char buffer[256];
    size_t total;
    int status;
    char *result;
    char *trimmed;

    pipefd[0] = -1;
    pipefd[1] = -1;
    pid = (-1);
    bytes_read = 0;
    total = 0;
    status = 0;
    result = NULL;
    trimmed = NULL;

    if (path == NULL) {
        return NULL;
    }

    if (pipe(pipefd) < 0) {
        return NULL;
    }

    pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        char const *argv[5];

        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
            _exit(127);
        }
        close(pipefd[1]);
        argv[0] = "file";
        argv[1] = "-b";
        argv[2] = "--mime-type";
        argv[3] = path;
        argv[4] = NULL;
        execvp("file", (char * const *)argv);
        _exit(127);
    }

    close(pipefd[1]);
    pipefd[1] = -1;
    total = 0;
    while ((bytes_read = read(pipefd[0], buffer + total,
                              sizeof(buffer) - total - 1)) > 0) {
        total += (size_t)bytes_read;
        if (total >= sizeof(buffer) - 1) {
            break;
        }
    }
    buffer[total] = '\0';
    close(pipefd[0]);
    pipefd[0] = -1;

    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        continue;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return NULL;
    }

    trimmed = thumbnailer_trim_left(buffer);
    thumbnailer_trim_right(trimmed);
    if (trimmed[0] == '\0') {
        return NULL;
    }

    result = thumbnailer_strdup(trimmed);

    return result;
}

/*
 * Thumbnailer supervision overview:
 *
 *     +-------------------+    pipe(stderr)    +-------------------+
 *     | libsixel parent   | <----------------- | thumbnailer argv  |
 *     |  - polls stdout   |                   |  - renders PNG     |
 *     |  - polls stderr   | -----------------> |  - returns code   |
 *     |  - waits pid      |    pipe(stdout)    |                   |
 *     +-------------------+  posix_spawn/fork  +-------------------+
 *
 * Non-blocking pipes keep verbose thumbnailers from stalling the loop,
 * and argv arrays mean Exec templates never pass through /bin/sh.
 */
/*
 * thumbnailer_spawn is responsible for preparing pipes, launching the
 * thumbnail helper, and streaming any emitted data back into libsixel.
 *
 *  - stderr is captured into stderr_output so verbose mode can replay the
 *    diagnostics without leaking them to non-verbose invocations.
 *  - stdout can optionally be redirected into a temporary file when
 *    thumbnailers insist on writing image data to the standard output stream.
 *  - All file descriptors are placed into non-blocking mode to avoid stalls
 *    while the parent waits for the child process to complete.
 * Arguments:
 *     command          - prepared argv array to execute.
 *     thumbnailer_name - friendly name used in log messages.
 *     log_prefix       - identifier describing the current pipeline step.
 *     capture_stdout   - non-zero to capture stdout into stdout_path.
 *     stdout_path      - file receiving stdout when capture_stdout != 0.
 * Returns:
 *     SIXEL_OK on success or a libsixel error code on failure.
 */
static SIXELSTATUS
thumbnailer_spawn(struct thumbnailer_command const *command,
                  char const *thumbnailer_name,
                  char const *log_prefix,
                  int capture_stdout,
                  char const *stdout_path)
{
    pid_t pid;
    int status_code;
    int wait_result;
    SIXELSTATUS status;
    char message[256];
    int stderr_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe_created;
    int stdout_pipe_created;
    int flags;
    ssize_t read_result;
    ssize_t stdout_read_result;
    char stderr_buffer[256];
    char stdout_buffer[4096];
    char *stderr_output;
    size_t stderr_length;
    size_t stderr_capacity;
    int child_exited;
    int stderr_open;
    int stdout_open;
    int have_status;
    int fatal_error;
    int output_fd;
    size_t write_offset;
    ssize_t write_result;
    size_t to_write;
    char const *display_command;
#if HAVE_POSIX_SPAWNP
    posix_spawn_file_actions_t actions;
    int spawn_result;
#endif

    pid = (-1);
    status_code = 0;
    wait_result = 0;
    status = SIXEL_RUNTIME_ERROR;
    memset(message, 0, sizeof(message));
    stderr_pipe[0] = -1;
    stderr_pipe[1] = -1;
    stdout_pipe[0] = -1;
    stdout_pipe[1] = -1;
    stderr_pipe_created = 0;
    stdout_pipe_created = 0;
    flags = 0;
    read_result = 0;
    stdout_read_result = 0;
    stderr_output = NULL;
    stderr_length = 0;
    stderr_capacity = 0;
    child_exited = 0;
    stderr_open = 0;
    stdout_open = 0;
    have_status = 0;
    fatal_error = 0;
    output_fd = -1;
    write_offset = 0;
    write_result = 0;
    to_write = 0;
    display_command = NULL;
#if HAVE_POSIX_SPAWNP
    spawn_result = 0;
#endif

    if (command == NULL || command->argv == NULL ||
            command->argv[0] == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (capture_stdout && stdout_path == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (capture_stdout) {
        output_fd = open(stdout_path,
                         O_WRONLY | O_CREAT | O_TRUNC,
                         0600);
        if (output_fd < 0) {
            if (!thumbnailer_safe_format(message,
                                         sizeof(message),
                                         "%s: open(%s) failed (%s).",
                                         log_prefix,
                                         stdout_path,
                                         strerror(errno))) {
                message[0] = '\0';
            }
            sixel_helper_set_additional_message(message);
            goto cleanup;
        }
    }

    /* stderr is collected even for successful runs so verbose users can see
     * the thumbnailer's own commentary.  Failing to set the pipe is not
     * fatal; we continue without stderr capture in that case.
     */
    if (pipe(stderr_pipe) == 0) {
        stderr_pipe_created = 1;
        stderr_open = 1;
    }

    if (capture_stdout) {
        if (pipe(stdout_pipe) != 0) {
            if (!thumbnailer_safe_format(message,
                                         sizeof(message),
                                         "%s: pipe() for stdout failed (%s).",
                                         log_prefix,
                                         strerror(errno))) {
                message[0] = '\0';
            }
            sixel_helper_set_additional_message(message);
            goto cleanup;
        }
        stdout_pipe_created = 1;
        stdout_open = 1;
    }

    display_command = (command->display != NULL) ?
            command->display : command->argv[0];
    loader_trace_message("%s: executing %s",
                         log_prefix,
                         display_command);

#if HAVE_POSIX_SPAWNP
    if (posix_spawn_file_actions_init(&actions) != 0) {
        if (!thumbnailer_safe_format(message,
                                     sizeof(message),
                                     "%s: posix_spawn_file_actions_init() "
                                     "failed.",
                                     log_prefix)) {
            message[0] = '\0';
        }
        sixel_helper_set_additional_message(message);
        goto cleanup;
    }
    if (stderr_pipe_created && stderr_pipe[1] >= 0) {
        (void)posix_spawn_file_actions_adddup2(&actions,
                                               stderr_pipe[1],
                                               STDERR_FILENO);
        (void)posix_spawn_file_actions_addclose(&actions,
                                                stderr_pipe[0]);
        (void)posix_spawn_file_actions_addclose(&actions,
                                                stderr_pipe[1]);
    }
    if (stdout_pipe_created && stdout_pipe[1] >= 0) {
        (void)posix_spawn_file_actions_adddup2(&actions,
                                               stdout_pipe[1],
                                               STDOUT_FILENO);
        (void)posix_spawn_file_actions_addclose(&actions,
                                                stdout_pipe[0]);
        (void)posix_spawn_file_actions_addclose(&actions,
                                                stdout_pipe[1]);
    }
    if (output_fd >= 0) {
        (void)posix_spawn_file_actions_addclose(&actions,
                                                output_fd);
    }
    spawn_result = posix_spawnp(&pid,
                                command->argv[0],
                                &actions,
                                NULL,
                                (char * const *)command->argv,
                                environ);
    posix_spawn_file_actions_destroy(&actions);
    if (spawn_result != 0) {
        if (!thumbnailer_safe_format(message,
                                     sizeof(message),
                                     "%s: posix_spawnp() failed (%s).",
                                     log_prefix,
                                     strerror(spawn_result))) {
            message[0] = '\0';
        }
        sixel_helper_set_additional_message(message);
        goto cleanup;
    }
#else
    pid = fork();
    if (pid < 0) {
        if (!thumbnailer_safe_format(message,
                                     sizeof(message),
                                     "%s: fork() failed (%s).",
                                     log_prefix,
                                     strerror(errno))) {
            message[0] = '\0';
        }
        sixel_helper_set_additional_message(message);
        goto cleanup;
    }
    if (pid == 0) {
        if (stderr_pipe_created && stderr_pipe[1] >= 0) {
            if (dup2(stderr_pipe[1], STDERR_FILENO) < 0) {
                _exit(127);
            }
        }
        if (stdout_pipe_created && stdout_pipe[1] >= 0) {
            if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0) {
                _exit(127);
            }
        }
        if (stderr_pipe[0] >= 0) {
            close(stderr_pipe[0]);
        }
        if (stderr_pipe[1] >= 0) {
            close(stderr_pipe[1]);
        }
        if (stdout_pipe[0] >= 0) {
            close(stdout_pipe[0]);
        }
        if (stdout_pipe[1] >= 0) {
            close(stdout_pipe[1]);
        }
        if (output_fd >= 0) {
            close(output_fd);
        }
        execvp(command->argv[0], (char * const *)command->argv);
        _exit(127);
    }
#endif

    loader_trace_message("%s: forked child pid=%ld",
                         log_prefix,
                         (long)pid);

    if (stderr_pipe_created && stderr_pipe[1] >= 0) {
        close(stderr_pipe[1]);
        stderr_pipe[1] = -1;
    }
    if (stdout_pipe_created && stdout_pipe[1] >= 0) {
        close(stdout_pipe[1]);
        stdout_pipe[1] = -1;
    }

    if (stderr_pipe_created && stderr_pipe[0] >= 0) {
        flags = fcntl(stderr_pipe[0], F_GETFL, 0);
        if (flags >= 0) {
            (void)fcntl(stderr_pipe[0],
                        F_SETFL,
                        flags | O_NONBLOCK);
        }
    }
    if (stdout_pipe_created && stdout_pipe[0] >= 0) {
        flags = fcntl(stdout_pipe[0], F_GETFL, 0);
        if (flags >= 0) {
            (void)fcntl(stdout_pipe[0],
                        F_SETFL,
                        flags | O_NONBLOCK);
        }
    }

    /* The monitoring loop drains stderr/stdout as long as any descriptor is
     * open.  Non-blocking reads ensure the parent does not deadlock if the
     * child process stalls or writes data in bursts.
     */
    while (!child_exited || stderr_open || stdout_open) {
        if (stderr_pipe_created && stderr_open) {
            read_result = read(stderr_pipe[0],
                               stderr_buffer,
                               (ssize_t)sizeof(stderr_buffer));
            if (read_result > 0) {
                if (stderr_length + (size_t)read_result + 1 >
                        stderr_capacity) {
                    size_t new_capacity;
                    char *new_output;

                    new_capacity = stderr_capacity;
                    if (new_capacity == 0) {
                        new_capacity = 256;
                    }
                    while (stderr_length + (size_t)read_result + 1 >
                            new_capacity) {
                        new_capacity *= 2U;
                    }
                    new_output = realloc(stderr_output, new_capacity);
                    if (new_output == NULL) {
                        free(stderr_output);
                        stderr_output = NULL;
                        stderr_capacity = 0;
                        stderr_length = 0;
                        stderr_open = 0;
                        if (stderr_pipe[0] >= 0) {
                            close(stderr_pipe[0]);
                            stderr_pipe[0] = -1;
                        }
                        stderr_pipe_created = 0;
                        if (!thumbnailer_safe_format(message,
                                                     sizeof(message),
                                                     "%s: realloc() failed.",
                                                     log_prefix)) {
                            message[0] = '\0';
                        }
                        sixel_helper_set_additional_message(message);
                        status = SIXEL_BAD_ALLOCATION;
                        fatal_error = 1;
                        break;
                    }
                    stderr_output = new_output;
                    stderr_capacity = new_capacity;
                }
                memcpy(stderr_output + stderr_length,
                       stderr_buffer,
                       (size_t)read_result);
                stderr_length += (size_t)read_result;
                stderr_output[stderr_length] = '\0';
            } else if (read_result == 0) {
                stderr_open = 0;
                if (stderr_pipe[0] >= 0) {
                    close(stderr_pipe[0]);
                    stderr_pipe[0] = -1;
                }
                stderr_pipe_created = 0;
            } else if (errno == EINTR) {
                /* retry */
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* no data */
            } else {
                if (!thumbnailer_safe_format(message,
                                             sizeof(message),
                                             "%s: read() failed (%s).",
                                             log_prefix,
                                             strerror(errno))) {
                    message[0] = '\0';
                }
                sixel_helper_set_additional_message(message);
                stderr_open = 0;
                if (stderr_pipe[0] >= 0) {
                    close(stderr_pipe[0]);
                    stderr_pipe[0] = -1;
                }
                stderr_pipe_created = 0;
            }
        }

        if (stdout_pipe_created && stdout_open) {
            stdout_read_result = read(stdout_pipe[0],
                                      stdout_buffer,
                                      (ssize_t)sizeof(stdout_buffer));
            if (stdout_read_result > 0) {
                write_offset = 0;
                while (write_offset < (size_t)stdout_read_result) {
                    to_write = (size_t)stdout_read_result - write_offset;
                    write_result = write(output_fd,
                                          stdout_buffer + write_offset,
                                          to_write);
                    if (write_result < 0) {
                        if (errno == EINTR) {
                            continue;
                        }
                        if (!thumbnailer_safe_format(message,
                                                     sizeof(message),
                                                     "%s: write() failed (%s).",
                                                     log_prefix,
                                                     strerror(errno))) {
                            message[0] = '\0';
                        }
                        sixel_helper_set_additional_message(message);
                        stdout_open = 0;
                        fatal_error = 1;
                        break;
                    }
                    write_offset += (size_t)write_result;
                }
            } else if (stdout_read_result == 0) {
                stdout_open = 0;
                if (stdout_pipe[0] >= 0) {
                    close(stdout_pipe[0]);
                    stdout_pipe[0] = -1;
                }
                stdout_pipe_created = 0;
            } else if (errno == EINTR) {
                /* retry */
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* no data */
            } else {
                if (!thumbnailer_safe_format(message,
                                             sizeof(message),
                                             "%s: read() failed (%s).",
                                             log_prefix,
                                             strerror(errno))) {
                    message[0] = '\0';
                }
                sixel_helper_set_additional_message(message);
                stdout_open = 0;
                if (stdout_pipe[0] >= 0) {
                    close(stdout_pipe[0]);
                    stdout_pipe[0] = -1;
                }
                stdout_pipe_created = 0;
            }
        }

        if (!child_exited) {
            wait_result = waitpid(pid, &status_code, WNOHANG);
            if (wait_result > 0) {
                child_exited = 1;
                have_status = 1;
            } else if (wait_result == 0) {
                /* child running */
            } else if (errno != EINTR) {
                if (!thumbnailer_safe_format(message,
                                             sizeof(message),
                                             "%s: waitpid() failed (%s).",
                                             log_prefix,
                                             strerror(errno))) {
                    message[0] = '\0';
                }
                sixel_helper_set_additional_message(message);
                status = SIXEL_RUNTIME_ERROR;
                fatal_error = 1;
                break;
            }
        }

        if (!child_exited || stderr_open || stdout_open) {
            thumbnailer_sleep_briefly();
        }
    }

    if (!child_exited) {
        do {
            wait_result = waitpid(pid, &status_code, 0);
        } while (wait_result < 0 && errno == EINTR);
        if (wait_result < 0) {
            if (!thumbnailer_safe_format(message,
                                         sizeof(message),
                                         "%s: waitpid() failed (%s).",
                                         log_prefix,
                                         strerror(errno))) {
                message[0] = '\0';
            }
            sixel_helper_set_additional_message(message);
            status = SIXEL_RUNTIME_ERROR;
            goto cleanup;
        }
        have_status = 1;
    }

    if (!have_status) {
        if (!thumbnailer_safe_format(message,
                                     sizeof(message),
                                     "%s: waitpid() failed (no status).",
                                     log_prefix)) {
            message[0] = '\0';
        }
        sixel_helper_set_additional_message(message);
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }

    if (!fatal_error) {
        if (WIFEXITED(status_code) && WEXITSTATUS(status_code) == 0) {
            status = SIXEL_OK;
            loader_trace_message("%s: child pid=%ld exited successfully",
                                 log_prefix,
                                 (long)pid);
        } else if (WIFEXITED(status_code)) {
            if (!thumbnailer_safe_format(message,
                                         sizeof(message),
                                         "%s: %s exited with status %d.",
                                         log_prefix,
                                         (thumbnailer_name != NULL) ?
                                         thumbnailer_name :
                                         "thumbnailer",
                                         WEXITSTATUS(status_code))) {
                message[0] = '\0';
            }
            sixel_helper_set_additional_message(message);
            status = SIXEL_RUNTIME_ERROR;
        } else if (WIFSIGNALED(status_code)) {
            if (!thumbnailer_safe_format(message,
                                         sizeof(message),
                                         "%s: %s terminated by signal %d.",
                                         log_prefix,
                                         (thumbnailer_name != NULL) ?
                                         thumbnailer_name :
                                         "thumbnailer",
                                         WTERMSIG(status_code))) {
                message[0] = '\0';
            }
            sixel_helper_set_additional_message(message);
            status = SIXEL_RUNTIME_ERROR;
        } else {
            if (!thumbnailer_safe_format(message,
                                         sizeof(message),
                                         "%s: %s exited abnormally.",
                                         log_prefix,
                                         (thumbnailer_name != NULL) ?
                                         thumbnailer_name :
                                         "thumbnailer")) {
                message[0] = '\0';
            }
            sixel_helper_set_additional_message(message);
            status = SIXEL_RUNTIME_ERROR;
        }
    }

cleanup:
    if (stderr_output != NULL && loader_trace_enabled &&
            stderr_length > 0) {
        loader_trace_message("%s: stderr:\n%s",
                             log_prefix,
                             stderr_output);
    }

    if (stderr_pipe[0] >= 0) {
        close(stderr_pipe[0]);
        stderr_pipe[0] = -1;
    }
    if (stderr_pipe[1] >= 0) {
        close(stderr_pipe[1]);
        stderr_pipe[1] = -1;
    }
    if (stdout_pipe[0] >= 0) {
        close(stdout_pipe[0]);
        stdout_pipe[0] = -1;
    }
    if (stdout_pipe[1] >= 0) {
        close(stdout_pipe[1]);
        stdout_pipe[1] = -1;
    }
    if (output_fd >= 0) {
        close(output_fd);
        output_fd = -1;
    }
    /* stderr_output accumulates all diagnostic text, so release it even when
     * verbose tracing is disabled.
     */
    free(stderr_output);

    return status;
}



/*
 * load_with_gnome_thumbnailer
 *
 * Drive the FreeDesktop thumbnailer pipeline and then decode the PNG
 * result using the built-in loader.
 *
 * GNOME thumbnail workflow overview:
 *
 *     +------------+    +-------------------+    +----------------+
 *     | source URI | -> | .thumbnailer Exec | -> | PNG thumbnail  |
 *     +------------+    +-------------------+    +----------------+
 *             |                    |                        |
 *             |                    v                        v
 *             |           spawn via /bin/sh         load_with_builtin()
 *             v
 *     file --mime-type
 *
 * Each step logs verbose breadcrumbs so integrators can diagnose which
 * thumbnailer matched, how the command was prepared, and why fallbacks
 * were selected.
 *
 * Arguments:
 *     pchunk        - source chunk representing the original document.
 *     fstatic       - image static-ness flag.
 *     fuse_palette  - palette usage flag.
 *     reqcolors     - requested colour count.
 *     bgcolor       - background colour override.
 *     loop_control  - animation loop control flag.
 *     fn_load       - downstream decoder callback.
 *     context       - user context forwarded to fn_load.
 * Returns:
 *     SIXEL_OK on success or libsixel error code on failure.
 */
static SIXELSTATUS
load_with_gnome_thumbnailer(
    sixel_chunk_t const       /* in */     *pchunk,
    int                       /* in */     fstatic,
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    sixel_load_image_function /* in */     fn_load,
    void                      /* in/out */ *context)
{
    SIXELSTATUS status;
    sixel_chunk_t *thumb_chunk;
    char template_path[] = "/tmp/libsixel-thumb-XXXXXX";
    char *png_path;
    size_t path_length;
    int fd;
    struct thumbnailer_string_list *directories;
    size_t dir_index;
    DIR *dir;
    struct dirent *entry;
    char *thumbnailer_path;
    struct thumbnailer_entry info;
    char *content_type;
    char *input_uri;
    struct thumbnailer_command *command;
    struct thumbnailer_command *evince_command;
    int executed;
    int command_success;
    int requested_size;
    char const *log_prefix;

    status = SIXEL_FALSE;
    thumb_chunk = NULL;
    png_path = NULL;
    path_length = 0;
    fd = -1;
    directories = NULL;
    dir_index = 0;
    dir = NULL;
    entry = NULL;
    thumbnailer_path = NULL;
    content_type = NULL;
    input_uri = NULL;
    command = NULL;
    evince_command = NULL;
    executed = 0;
    command_success = 0;
    log_prefix = "load_with_gnome_thumbnailer";
    requested_size = thumbnailer_size_hint;
    if (requested_size <= 0) {
        requested_size = SIXEL_THUMBNAILER_DEFAULT_SIZE;
    }

    loader_trace_message("%s: thumbnail size hint=%d",
                         log_prefix,
                         requested_size);

    thumbnailer_entry_init(&info);

    if (pchunk->source_path == NULL) {
        sixel_helper_set_additional_message(
            "load_with_gnome_thumbnailer: source path is unavailable.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    fd = mkstemp(template_path);
    if (fd < 0) {
        sixel_helper_set_additional_message(
            "load_with_gnome_thumbnailer: mkstemp() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }
    close(fd);
    fd = -1;

    path_length = strlen(template_path) + 5;
    png_path = malloc(path_length);
    if (png_path == NULL) {
        sixel_helper_set_additional_message(
            "load_with_gnome_thumbnailer: malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        unlink(template_path);
        goto end;
    }
    if (!thumbnailer_safe_format(png_path,
                                 path_length,
                                 "%s.png",
                                 template_path)) {
        if (path_length > 0) {
            png_path[path_length - 1] = '\0';
        }
    }
    if (rename(template_path, png_path) != 0) {
        sixel_helper_set_additional_message(
            "load_with_gnome_thumbnailer: rename() failed.");
        status = SIXEL_RUNTIME_ERROR;
        unlink(template_path);
        goto end;
    }

    content_type = thumbnailer_guess_content_type(pchunk->source_path);
    input_uri = thumbnailer_build_file_uri(pchunk->source_path);

    loader_trace_message("%s: detected MIME type %s for %s",
                         log_prefix,
                         (content_type != NULL) ? content_type :
                         "(unknown)",
                         pchunk->source_path);

    directories = thumbnailer_collect_directories();
    if (directories == NULL) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    /* Iterate through every configured thumbnailer directory so we honour
     * overrides in $HOME as well as desktop environment defaults discovered
     * through XDG_DATA_DIRS.
     */
    for (dir_index = 0; dir_index < directories->length; ++dir_index) {
        loader_trace_message("%s: checking thumbnailers in %s",
                             log_prefix,
                             directories->items[dir_index]);

        dir = opendir(directories->items[dir_index]);
        if (dir == NULL) {
            continue;
        }
        while ((entry = readdir(dir)) != NULL) {
            thumbnailer_entry_clear(&info);
            thumbnailer_entry_init(&info);
            size_t name_length;

            name_length = strlen(entry->d_name);
            if (name_length < 12 ||
                    strcmp(entry->d_name + name_length - 12,
                           ".thumbnailer") != 0) {
                continue;
            }
            thumbnailer_path = thumbnailer_join_paths(
                directories->items[dir_index],
                entry->d_name);
            if (thumbnailer_path == NULL) {
                continue;
            }
            if (!thumbnailer_parse_file(thumbnailer_path, &info)) {
                free(thumbnailer_path);
                thumbnailer_path = NULL;
                continue;
            }
            free(thumbnailer_path);
            thumbnailer_path = NULL;
            loader_trace_message(
                "%s: parsed %s (TryExec=%s)",
                log_prefix,
                entry->d_name,
                (info.tryexec != NULL) ? info.tryexec : "(none)");
            if (content_type == NULL) {
                continue;
            }
            if (!thumbnailer_has_tryexec(info.tryexec)) {
                loader_trace_message("%s: skipping %s (TryExec missing)",
                                     log_prefix,
                                     entry->d_name);
                continue;
            }
            if (!thumbnailer_supports_mime(&info, content_type)) {
                loader_trace_message("%s: %s does not support %s",
                                     log_prefix,
                                     entry->d_name,
                                     content_type);
                continue;
            }
            if (info.exec_line == NULL) {
                continue;
            }
            loader_trace_message("%s: %s supports %s with Exec=\"%s\"",
                                 log_prefix,
                                 entry->d_name,
                                 content_type,
                                 info.exec_line);
            loader_trace_message("%s: preparing %s for %s",
                                 log_prefix,
                                 entry->d_name,
                                 content_type);
            command = thumbnailer_build_command(info.exec_line,
                                                pchunk->source_path,
                                                input_uri,
                                                png_path,
                                                requested_size,
                                                content_type);
            if (command == NULL) {
                continue;
            }
            if (thumbnailer_is_evince_thumbnailer(info.exec_line,
                                                  info.tryexec)) {
                loader_trace_message(
                    "%s: applying evince-thumbnailer stdout workaround",
                    log_prefix);
                /* evince-thumbnailer fails when passed an output path.
                 * Redirect stdout and copy the stream instead.
                 */
                evince_command = thumbnailer_build_evince_command(
                    pchunk->source_path,
                    requested_size);
                if (evince_command == NULL) {
                    thumbnailer_command_free(command);
                    command = NULL;
                    continue;
                }
                thumbnailer_command_free(command);
                command = evince_command;
                evince_command = NULL;
                unlink(png_path);
                status = thumbnailer_spawn(command,
                                           entry->d_name,
                                           log_prefix,
                                           1,
                                           png_path);
            } else {
                unlink(png_path);
                status = thumbnailer_spawn(command,
                                           entry->d_name,
                                           log_prefix,
                                           0,
                                           NULL);
            }
            thumbnailer_command_free(command);
            command = NULL;
            executed = 1;
            if (SIXEL_SUCCEEDED(status)) {
                command_success = 1;
                loader_trace_message("%s: %s produced %s",
                                     log_prefix,
                                     entry->d_name,
                                     png_path);
                break;
            }
        }
        closedir(dir);
        dir = NULL;
        if (command_success) {
            break;
        }
    }

    if (!command_success) {
        loader_trace_message("%s: falling back to gdk-pixbuf-thumbnailer",
                             log_prefix);
        unlink(png_path);
        command = thumbnailer_build_command(
            "gdk-pixbuf-thumbnailer --size=%s %i %o",
            pchunk->source_path,
            input_uri,
            png_path,
            requested_size,
            content_type);
        if (command != NULL) {
            unlink(png_path);
            status = thumbnailer_spawn(command,
                                       "gdk-pixbuf-thumbnailer",
                                       log_prefix,
                                       0,
                                       NULL);
            thumbnailer_command_free(command);
            command = NULL;
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            executed = 1;
            command_success = 1;
            loader_trace_message("%s: gdk-pixbuf-thumbnailer produced %s",
                                 log_prefix,
                                 png_path);
        }
    }

    if (!executed) {
        sixel_helper_set_additional_message(
            "load_with_gnome_thumbnailer: no thumbnailer available.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    status = sixel_chunk_new(&thumb_chunk,
                             png_path,
                             0,
                             NULL,
                             pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = load_with_builtin(thumb_chunk,
                               fstatic,
                               fuse_palette,
                               reqcolors,
                               bgcolor,
                               loop_control,
                               fn_load,
                               context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
    if (command != NULL) {
        thumbnailer_command_free(command);
        command = NULL;
    }
    if (evince_command != NULL) {
        thumbnailer_command_free(evince_command);
        evince_command = NULL;
    }
    if (thumb_chunk != NULL) {
        sixel_chunk_destroy(thumb_chunk);
        thumb_chunk = NULL;
    }
    if (png_path != NULL) {
        unlink(png_path);
        free(png_path);
        png_path = NULL;
    }
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
    if (directories != NULL) {
        thumbnailer_string_list_free(directories);
        directories = NULL;
    }
    if (dir != NULL) {
        closedir(dir);
        dir = NULL;
    }
    thumbnailer_entry_clear(&info);
    free(content_type);
    content_type = NULL;
    free(input_uri);
    input_uri = NULL;

    return status;
}

#endif  /* HAVE_UNISTD_H && HAVE_SYS_WAIT_H && HAVE_FORK */


#if HAVE_GD
static int
detect_file_format(int len, unsigned char *data)
{
    if (len > 18 && memcmp("TRUEVISION", data + len - 18, 10) == 0) {
        return SIXEL_FORMAT_TGA;
    }

    if (len > 3 && memcmp("GIF", data, 3) == 0) {
        return SIXEL_FORMAT_GIF;
    }

    if (len > 8 && memcmp("\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", data, 8) == 0) {
        return SIXEL_FORMAT_PNG;
    }

    if (len > 2 && memcmp("BM", data, 2) == 0) {
        return SIXEL_FORMAT_BMP;
    }

    if (len > 2 && memcmp("\xFF\xD8", data, 2) == 0) {
        return SIXEL_FORMAT_JPG;
    }

    if (len > 2 && memcmp("\x00\x00", data, 2) == 0) {
        return SIXEL_FORMAT_WBMP;
    }

    if (len > 2 && memcmp("\x4D\x4D", data, 2) == 0) {
        return SIXEL_FORMAT_TIFF;
    }

    if (len > 2 && memcmp("\x49\x49", data, 2) == 0) {
        return SIXEL_FORMAT_TIFF;
    }

    if (len > 2 && memcmp("\033P", data, 2) == 0) {
        return SIXEL_FORMAT_SIXEL;
    }

    if (len > 2 && data[0] == 0x90  && (data[len - 1] == 0x9C || data[len - 2] == 0x9C)) {
        return SIXEL_FORMAT_SIXEL;
    }

    if (len > 1 && data[0] == 'P' && data[1] >= '1' && data[1] <= '6') {
        return SIXEL_FORMAT_PNM;
    }

    if (len > 3 && memcmp("gd2", data, 3) == 0) {
        return SIXEL_FORMAT_GD2;
    }

    if (len > 4 && memcmp("8BPS", data, 4) == 0) {
        return SIXEL_FORMAT_PSD;
    }

    if (len > 11 && memcmp("#?RADIANCE\n", data, 11) == 0) {
        return SIXEL_FORMAT_HDR;
    }

    return (-1);
}
#endif /* HAVE_GD */

#if HAVE_GD

static SIXELSTATUS
load_with_gd(
    sixel_chunk_t const       /* in */     *pchunk,      /* image data */
    int                       /* in */     fstatic,      /* static */
    int                       /* in */     fuse_palette, /* whether to use palette if possible */
    int                       /* in */     reqcolors,    /* reqcolors */
    unsigned char             /* in */     *bgcolor,     /* background color */
    int                       /* in */     loop_control, /* one of enum loop_control */
    sixel_load_image_function /* in */     fn_load,      /* callback */
    void                      /* in/out */ *context      /* private data for callback */
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char *p;
    gdImagePtr im = NULL;
    int x, y;
    int c;
    sixel_frame_t *frame = NULL;
    int format;

    (void) fstatic;
    (void) fuse_palette;
    (void) reqcolors;
    (void) bgcolor;
    (void) loop_control;

    format = detect_file_format(pchunk->size, pchunk->buffer);

    if (format == SIXEL_FORMAT_GIF) {
#if HAVE_DECL_GDIMAGECREATEFROMGIFANIMPTR
        gdImagePtr *ims = NULL;
        int frames = 0;
        int i;
        int *delays = NULL;

        ims = gdImageCreateFromGifAnimPtr(pchunk->size, pchunk->buffer,
                                          &frames, &delays);
        if (ims == NULL) {
            status = SIXEL_GD_ERROR;
            goto end;
        }

        for (i = 0; i < frames; i++) {
            im = ims[i];
            if (!gdImageTrueColor(im)) {
# if HAVE_DECL_GDIMAGEPALETTETOTRUECOLOR
                if (!gdImagePaletteToTrueColor(im)) {
                    status = SIXEL_GD_ERROR;
                    goto gif_end;
                }
# else
                status = SIXEL_GD_ERROR;
                goto gif_end;
# endif
            }

            status = sixel_frame_new(&frame, pchunk->allocator);
            if (SIXEL_FAILED(status)) {
                frame = NULL;
                goto gif_end;
            }

            frame->width = gdImageSX(im);
            frame->height = gdImageSY(im);
            frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
            p = frame->pixels = sixel_allocator_malloc(
                pchunk->allocator,
                (size_t)(frame->width * frame->height * 3));
            if (frame->pixels == NULL) {
                sixel_helper_set_additional_message(
                    "load_with_gd: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                sixel_frame_unref(frame);
                frame = NULL;
                goto gif_end;
            }
            for (y = 0; y < frame->height; y++) {
                for (x = 0; x < frame->width; x++) {
                    c = gdImageTrueColorPixel(im, x, y);
                    *p++ = gdTrueColorGetRed(c);
                    *p++ = gdTrueColorGetGreen(c);
                    *p++ = gdTrueColorGetBlue(c);
                }
            }

            if (delays) {
                frame->delay.tv_sec = delays[i] / 100;
                frame->delay.tv_nsec = (delays[i] % 100) * 10000000L;
            }

            status = fn_load(frame, context);
            sixel_frame_unref(frame);
            frame = NULL;
            gdImageDestroy(im);
            ims[i] = NULL;
            if (SIXEL_FAILED(status)) {
                goto gif_end;
            }
        }

        status = SIXEL_OK;

gif_end:
        if (delays) {
            gdFree(delays);
        }
        if (ims) {
            for (i = 0; i < frames; i++) {
                if (ims[i]) {
                    gdImageDestroy(ims[i]);
                }
            }
            gdFree(ims);
        }
        goto end;
#else
        status = SIXEL_GD_ERROR;
        goto end;
#endif
    }

    switch (format) {
#if HAVE_DECL_GDIMAGECREATEFROMPNGPTR
        case SIXEL_FORMAT_PNG:
            im = gdImageCreateFromPngPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMPNGPTR */
#if HAVE_DECL_GDIMAGECREATEFROMBMPPTR
        case SIXEL_FORMAT_BMP:
            im = gdImageCreateFromBmpPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMBMPPTR */
        case SIXEL_FORMAT_JPG:
#if HAVE_DECL_GDIMAGECREATEFROMJPEGPTREX
            im = gdImageCreateFromJpegPtrEx(pchunk->size, pchunk->buffer, 1);
#elif HAVE_DECL_GDIMAGECREATEFROMJPEGPTR
            im = gdImageCreateFromJpegPtr(pchunk->size, pchunk->buffer);
#endif  /* HAVE_DECL_GDIMAGECREATEFROMJPEGPTREX */
            break;
#if HAVE_DECL_GDIMAGECREATEFROMTGAPTR
        case SIXEL_FORMAT_TGA:
            im = gdImageCreateFromTgaPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMTGAPTR */
#if HAVE_DECL_GDIMAGECREATEFROMWBMPPTR
        case SIXEL_FORMAT_WBMP:
            im = gdImageCreateFromWBMPPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMWBMPPTR */
#if HAVE_DECL_GDIMAGECREATEFROMTIFFPTR
        case SIXEL_FORMAT_TIFF:
            im = gdImageCreateFromTiffPtr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMTIFFPTR */
#if HAVE_DECL_GDIMAGECREATEFROMGD2PTR
        case SIXEL_FORMAT_GD2:
            im = gdImageCreateFromGd2Ptr(pchunk->size, pchunk->buffer);
            break;
#endif  /* HAVE_DECL_GDIMAGECREATEFROMGD2PTR */
        default:
            status = SIXEL_GD_ERROR;
            sixel_helper_set_additional_message(
                "unexpected image format detected.");
            goto end;
    }

    if (im == NULL) {
        status = SIXEL_GD_ERROR;
        /* TODO: retrieve error detail */
        goto end;
    }

    if (!gdImageTrueColor(im)) {
#if HAVE_DECL_GDIMAGEPALETTETOTRUECOLOR
        if (!gdImagePaletteToTrueColor(im)) {
            gdImageDestroy(im);
            status = SIXEL_GD_ERROR;
            /* TODO: retrieve error detail */
            goto end;
        }
#else
        status = SIXEL_GD_ERROR;
        /* TODO: retrieve error detail */
        goto end;
#endif
    }

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    frame->width = gdImageSX(im);
    frame->height = gdImageSY(im);
    frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    p = frame->pixels = sixel_allocator_malloc(
        pchunk->allocator, (size_t)(frame->width * frame->height * 3));
    if (frame->pixels == NULL) {
        sixel_helper_set_additional_message(
            "load_with_gd: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        gdImageDestroy(im);
        goto end;
    }
    for (y = 0; y < frame->height; y++) {
        for (x = 0; x < frame->width; x++) {
            c = gdImageTrueColorPixel(im, x, y);
            *p++ = gdTrueColorGetRed(c);
            *p++ = gdTrueColorGetGreen(c);
            *p++ = gdTrueColorGetBlue(c);
        }
    }
    gdImageDestroy(im);

    status = fn_load(frame, context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    sixel_frame_unref(frame);

    status = SIXEL_OK;

end:
    return status;
}

#endif  /* HAVE_GD */

#if HAVE_WIC

#include <windows.h>
#include <wincodec.h>

SIXELSTATUS
load_with_wic(
    sixel_chunk_t const       /* in */     *pchunk,      /* image data */
    int                       /* in */     fstatic,      /* static */
    int                       /* in */     fuse_palette, /* whether to use palette if possible */
    int                       /* in */     reqcolors,    /* reqcolors */
    unsigned char             /* in */     *bgcolor,     /* background color */
    int                       /* in */     loop_control, /* one of enum loop_control */
    sixel_load_image_function /* in */     fn_load,      /* callback */
    void                      /* in/out */ *context      /* private data for callback */
)
{
    HRESULT                 hr         = E_FAIL;
    SIXELSTATUS             status     = SIXEL_FALSE;
    IWICImagingFactory     *factory    = NULL;
    IWICStream             *stream     = NULL;
    IWICBitmapDecoder      *decoder    = NULL;
    IWICBitmapFrameDecode  *wicframe   = NULL;
    IWICFormatConverter    *conv       = NULL;
    IWICBitmapSource       *src        = NULL;
    IWICPalette            *wicpalette = NULL;
    WICColor               *wiccolors  = NULL;
    IWICMetadataQueryReader *qdecoder  = NULL;
    IWICMetadataQueryReader *qframe    = NULL;
    UINT                    ncolors    = 0;
    sixel_frame_t          *frame      = NULL;
    int                     comp       = 4;
    UINT                    actual     = 0;
    UINT                    i;
    UINT                    frame_count = 0;
    int                     anim_loop_count = (-1);
    int                     is_gif;
    WICColor                c;

    (void) reqcolors;
    (void) bgcolor;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return status;
    }

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IWICImagingFactory, (void**)&factory);
    if (FAILED(hr)) {
        goto end;
    }

    hr = factory->lpVtbl->CreateStream(factory, &stream);
    if (FAILED(hr)) {
        goto end;
    }

    hr = stream->lpVtbl->InitializeFromMemory(stream,
                                              (BYTE*)pchunk->buffer,
                                              (DWORD)pchunk->size);
    if (FAILED(hr)) {
        goto end;
    }

    hr = factory->lpVtbl->CreateDecoderFromStream(factory,
                                                  (IStream*)stream,
                                                  NULL,
                                                  WICDecodeMetadataCacheOnDemand,
                                                  &decoder);
    if (FAILED(hr)) {
        goto end;
    }

    is_gif = (memcmp("GIF", pchunk->buffer, 3) == 0);

    if (is_gif) {
        hr = decoder->lpVtbl->GetFrameCount(decoder, &frame_count);
        if (FAILED(hr)) {
            goto end;
        }
        if (fstatic) {
            frame_count = 1;
        }

        hr = decoder->lpVtbl->GetMetadataQueryReader(decoder, &qdecoder);
        if (SUCCEEDED(hr)) {
            PROPVARIANT pv;
            PropVariantInit(&pv);
            hr = qdecoder->lpVtbl->GetMetadataByName(
                qdecoder,
                L"/appext/Application/NETSCAPE2.0/Loop",
                &pv);
            if (SUCCEEDED(hr) && pv.vt == VT_UI2) {
                anim_loop_count = pv.uiVal;
            }
            PropVariantClear(&pv);
            qdecoder->lpVtbl->Release(qdecoder);
            qdecoder = NULL;
        }

        frame->loop_count = 0;
        for (;;) {
            frame->frame_no = 0;
            for (i = 0; i < frame_count; ++i) {
                hr = decoder->lpVtbl->GetFrame(decoder, i, &wicframe);
                if (FAILED(hr)) {
                    goto end;
                }

                hr = factory->lpVtbl->CreateFormatConverter(factory, &conv);
                if (FAILED(hr)) {
                    goto end;
                }
                hr = conv->lpVtbl->Initialize(conv,
                                              (IWICBitmapSource*)wicframe,
                                              &GUID_WICPixelFormat32bppRGBA,
                                              WICBitmapDitherTypeNone,
                                              NULL,
                                              0.0,
                                              WICBitmapPaletteTypeCustom);
                if (FAILED(hr)) {
                    goto end;
                }

                src = (IWICBitmapSource*)conv;
                comp = 4;
                frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;

                hr = src->lpVtbl->GetSize(
                    src,
                    (UINT *)&frame->width,
                    (UINT *)&frame->height);
                if (FAILED(hr)) {
                    goto end;
                }

                if (frame->width <= 0 || frame->height <= 0 ||
                    frame->width > SIXEL_WIDTH_LIMIT ||
                    frame->height > SIXEL_HEIGHT_LIMIT) {
                    sixel_helper_set_additional_message(
                        "load_with_wic: an invalid width or height parameter detected.");
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }

                frame->pixels = sixel_allocator_malloc(
                    pchunk->allocator,
                    (size_t)(frame->height * frame->width * comp));
                if (frame->pixels == NULL) {
                    hr = E_OUTOFMEMORY;
                    goto end;
                }

                {
                    WICRect rc = { 0, 0, (INT)frame->width, (INT)frame->height };
                    hr = src->lpVtbl->CopyPixels(
                        src,
                        &rc,
                        frame->width * comp,
                        (UINT)frame->width * frame->height * comp,
                        frame->pixels);
                    if (FAILED(hr)) {
                        goto end;
                    }
                }

                frame->delay = 0;
                hr = wicframe->lpVtbl->GetMetadataQueryReader(wicframe, &qframe);
                if (SUCCEEDED(hr)) {
                    PROPVARIANT pv;
                    PropVariantInit(&pv);
                    hr = qframe->lpVtbl->GetMetadataByName(
                        qframe,
                        L"/grctlext/Delay",
                        &pv);
                    if (SUCCEEDED(hr) && pv.vt == VT_UI2) {
                        frame->delay = (int)(pv.uiVal) * 10;
                    }
                    PropVariantClear(&pv);
                    qframe->lpVtbl->Release(qframe);
                    qframe = NULL;
                }

                frame->multiframe = 1;
                status = fn_load(frame, context);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                frame->pixels = NULL;
                frame->palette = NULL;

                if (conv) {
                    conv->lpVtbl->Release(conv);
                    conv = NULL;
                }
                if (wicframe) {
                    wicframe->lpVtbl->Release(wicframe);
                    wicframe = NULL;
                }

                frame->frame_no++;
            }

            ++frame->loop_count;

            if (anim_loop_count < 0) {
                break;
            }
            if (loop_control == SIXEL_LOOP_DISABLE || frame->frame_no == 1) {
                break;
            }
            if (loop_control == SIXEL_LOOP_AUTO &&
                frame->loop_count == anim_loop_count) {
                break;
            }
        }

        status = SIXEL_OK;
        goto end;
    }

    hr = decoder->lpVtbl->GetFrame(decoder, 0, &wicframe);
    if (FAILED(hr)) {
        goto end;
    }

    if (fuse_palette) {
        hr = factory->lpVtbl->CreatePalette(factory, &wicpalette);
        if (SUCCEEDED(hr)) {
            hr = wicframe->lpVtbl->CopyPalette(wicframe, wicpalette);
        }
        if (SUCCEEDED(hr)) {
            hr = wicpalette->lpVtbl->GetColorCount(wicpalette, &ncolors);
        }
        if (SUCCEEDED(hr) && ncolors > 0 && ncolors <= 256) {
            hr = factory->lpVtbl->CreateFormatConverter(factory, &conv);
            if (SUCCEEDED(hr)) {
                hr = conv->lpVtbl->Initialize(conv,
                                              (IWICBitmapSource*)wicframe,
                                              &GUID_WICPixelFormat8bppIndexed,
                                              WICBitmapDitherTypeNone,
                                              wicpalette,
                                              0.0,
                                              WICBitmapPaletteTypeCustom);
            }
            if (SUCCEEDED(hr)) {
                src = (IWICBitmapSource*)conv;
                comp = 1;
                frame->pixelformat = SIXEL_PIXELFORMAT_PAL8;
                frame->palette = sixel_allocator_malloc(
                    pchunk->allocator,
                    (size_t)ncolors * 3);
                if (frame->palette == NULL) {
                    hr = E_OUTOFMEMORY;
                } else {
                    wiccolors = (WICColor *)sixel_allocator_malloc(
                        pchunk->allocator,
                        (size_t)ncolors * sizeof(WICColor));
                    if (wiccolors == NULL) {
                        hr = E_OUTOFMEMORY;
                    } else {
                        actual = 0;
                        hr = wicpalette->lpVtbl->GetColors(
                            wicpalette, ncolors, wiccolors, &actual);
                        if (SUCCEEDED(hr) && actual == ncolors) {
                            for (i = 0; i < ncolors; ++i) {
                                c = wiccolors[i];
                                frame->palette[i * 3 + 0] = (unsigned char)((c >> 16) & 0xFF);
                                frame->palette[i * 3 + 1] = (unsigned char)((c >> 8) & 0xFF);
                                frame->palette[i * 3 + 2] = (unsigned char)(c & 0xFF);
                            }
                            frame->ncolors = (int)ncolors;
                        } else {
                            hr = E_FAIL;
                        }
                    }
                }
            }
            if (FAILED(hr)) {
                if (conv) {
                    conv->lpVtbl->Release(conv);
                    conv = NULL;
                }
                sixel_allocator_free(pchunk->allocator, frame->palette);
                frame->palette = NULL;
                sixel_allocator_free(pchunk->allocator, wiccolors);
                wiccolors = NULL;
                src = NULL;
            }
        }
    }

    if (src == NULL) {
        hr = factory->lpVtbl->CreateFormatConverter(factory, &conv);
        if (FAILED(hr)) {
            goto end;
        }

        hr = conv->lpVtbl->Initialize(conv, (IWICBitmapSource*)wicframe,
                                      &GUID_WICPixelFormat32bppRGBA,
                                      WICBitmapDitherTypeNone, NULL, 0.0,
                                      WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) {
            goto end;
        }

        src = (IWICBitmapSource*)conv;
        comp = 4;
        frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    }

    hr = src->lpVtbl->GetSize(
        src, (UINT *)&frame->width, (UINT *)&frame->height);
    if (FAILED(hr)) {
        goto end;
    }

    /* check size */
    if (frame->width <= 0) {
        sixel_helper_set_additional_message(
            "load_with_wic: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (frame->height <= 0) {
        sixel_helper_set_additional_message(
            "load_with_wic: an invalid width parameter detected.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (frame->width > SIXEL_WIDTH_LIMIT) {
        sixel_helper_set_additional_message(
            "load_with_wic: given width parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (frame->height > SIXEL_HEIGHT_LIMIT) {
        sixel_helper_set_additional_message(
            "load_with_wic: given height parameter is too huge.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    frame->pixels = sixel_allocator_malloc(
        pchunk->allocator,
        (size_t)(frame->height * frame->width * comp));

    {
        WICRect rc = { 0, 0, (INT)frame->width, (INT)frame->height };
        hr = src->lpVtbl->CopyPixels(
            src,
            &rc,                                        /* prc */
            frame->width * comp,                        /* cbStride */
            (UINT)frame->width * frame->height * comp,  /* cbBufferSize */
            frame->pixels);                             /* pbBuffer */
        if (FAILED(hr)) {
            goto end;
        }
    }

    status = fn_load(frame, context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    if (conv) {
         conv->lpVtbl->Release(conv);
    }
    if (wicpalette) {
         wicpalette->lpVtbl->Release(wicpalette);
    }
    if (wiccolors) {
         sixel_allocator_free(pchunk->allocator, wiccolors);
    }
    if (wicframe) {
         wicframe->lpVtbl->Release(wicframe);
    }
    if (qdecoder) {
         qdecoder->lpVtbl->Release(qdecoder);
    }
    if (qframe) {
         qframe->lpVtbl->Release(qframe);
    }
    if (stream) {
         stream->lpVtbl->Release(stream);
    }
    if (factory) {
         factory->lpVtbl->Release(factory);
    }
    sixel_frame_unref(frame);

    CoUninitialize();

    if (FAILED(hr)) {
        return SIXEL_FALSE;
    }

    return SIXEL_OK;
}

#endif /* HAVE_WIC */

/* load image from file */

SIXELAPI SIXELSTATUS
sixel_helper_load_image_file(
    char const                /* in */     *filename,     /* source file name */
    int                       /* in */     fstatic,       /* whether to extract static image from animated gif */
    int                       /* in */     fuse_palette,  /* whether to use paletted image, set non-zero value to try to get paletted image */
    int                       /* in */     reqcolors,     /* requested number of colors, should be equal or less than SIXEL_PALETTE_MAX */
    unsigned char             /* in */     *bgcolor,      /* background color, may be NULL */
    int                       /* in */     loop_control,  /* one of enum loopControl */
    sixel_load_image_function /* in */     fn_load,       /* callback */
    int                       /* in */     finsecure,     /* true if do not verify SSL */
    int const                 /* in */     *cancel_flag,  /* cancel flag, may be NULL */
    void                      /* in/out */ *context,      /* private data which is passed to callback function as an argument, may be NULL */
    sixel_allocator_t         /* in */     *allocator     /* allocator object, may be NULL */
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_chunk_t *pchunk = NULL;

    /* normalize reqested colors */
    if (reqcolors > SIXEL_PALETTE_MAX) {
        reqcolors = SIXEL_PALETTE_MAX;
    }

    /* create new chunk object from file */
    status = sixel_chunk_new(&pchunk, filename, finsecure, cancel_flag, allocator);
    if (status != SIXEL_OK) {
        goto end;
    }

    /* if input date is empty or 1 byte LF, ignore it and return successfully */
    if (pchunk->size == 0 || (pchunk->size == 1 && *pchunk->buffer == '\n')) {
        status = SIXEL_OK;
        goto end;
    }

    /* assertion */
    if (pchunk->buffer == NULL || pchunk->max_size == 0) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }

    status = SIXEL_FALSE;
#ifdef HAVE_WIC
    if (SIXEL_FAILED(status) && !chunk_is_gif(pchunk)) {
        loader_trace_try("wic");
        status = load_with_wic(pchunk,
                               fstatic,
                               fuse_palette,
                               reqcolors,
                               bgcolor,
                               loop_control,
                               fn_load,
                               context);
        loader_trace_result("wic", status);
    }
#endif  /* HAVE_WIC */
#ifdef HAVE_COREGRAPHICS
    if (SIXEL_FAILED(status)) {
        loader_trace_try("coregraphics");
        status = load_with_coregraphics(pchunk,
                                        fstatic,
                                        fuse_palette,
                                        reqcolors,
                                        bgcolor,
                                        loop_control,
                                        fn_load,
                                        context);
        loader_trace_result("coregraphics", status);
    }
#endif  /* HAVE_COREGRAPHICS */
#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
    if (SIXEL_FAILED(status)) {
        loader_trace_try("quicklook");
        status = load_with_quicklook(pchunk,
                                     fstatic,
                                     fuse_palette,
                                     reqcolors,
                                     bgcolor,
                                     loop_control,
                                     fn_load,
                                     context);
        loader_trace_result("quicklook", status);
    }
#endif  /* HAVE_COREGRAPHICS && HAVE_QUICKLOOK */
#ifdef HAVE_GDK_PIXBUF2
    if (SIXEL_FAILED(status)) {
        loader_trace_try("gdk-pixbuf2");
        status = load_with_gdkpixbuf(pchunk,
                                     fstatic,
                                     fuse_palette,
                                     reqcolors,
                                     bgcolor,
                                     loop_control,
                                     fn_load,
                                     context);
        loader_trace_result("gdk-pixbuf2", status);
    }
#endif  /* HAVE_GDK_PIXBUF2 */
#if HAVE_GD
    if (SIXEL_FAILED(status)) {
        loader_trace_try("gd");
        status = load_with_gd(pchunk,
                              fstatic,
                              fuse_palette,
                              reqcolors,
                              bgcolor,
                              loop_control,
                              fn_load,
                              context);
        loader_trace_result("gd", status);
    }
#endif  /* HAVE_GD */
    if (SIXEL_FAILED(status)) {
        loader_trace_try("builtin");
        status = load_with_builtin(pchunk,
                                   fstatic,
                                   fuse_palette,
                                   reqcolors,
                                   bgcolor,
                                   loop_control,
                                   fn_load,
                                   context);
        loader_trace_result("builtin", status);
    }
#if HAVE_UNISTD_H && HAVE_SYS_WAIT_H && HAVE_FORK
    if (SIXEL_FAILED(status)) {
        loader_trace_try("gnome-thumbnailer");
        status = load_with_gnome_thumbnailer(pchunk,
                                             fstatic,
                                             fuse_palette,
                                             reqcolors,
                                             bgcolor,
                                             loop_control,
                                             fn_load,
                                             context);
        loader_trace_result("gnome-thumbnailer", status);
    }
#endif
    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    sixel_chunk_destroy(pchunk);

    return status;
}


#if HAVE_TESTS
static int
test1(void)
{
    int nret = EXIT_FAILURE;
    unsigned char *ptr = malloc(16);

    nret = EXIT_SUCCESS;
    goto error;

    nret = EXIT_SUCCESS;

error:
    free(ptr);
    return nret;
}


SIXELAPI int
sixel_loader_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (* testcase)(void);

    static testcase const testcases[] = {
        test1,
    };

    for (i = 0; i < sizeof(testcases) / sizeof(testcase); ++i) {
        nret = testcases[i]();
        if (nret != EXIT_SUCCESS) {
            goto error;
        }
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}
#endif  /* HAVE_TESTS */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
