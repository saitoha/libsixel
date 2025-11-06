/*
 * Copyright (c) 2014-2018 Hayaki Saito
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

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif  /* HAVE_STRING_H */
#if HAVE_SETJMP_H
# include <setjmp.h>
#endif  /* HAVE_SETJMP_H */
#if HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */
#if HAVE_LIBPNG
# include <png.h>
#else
# if !defined(HAVE_MEMCPY)
#  define memcpy(d, s, n) (bcopy ((s), (d), (n)))
# endif
# if !defined(HAVE_MEMMOVE)
#  define memmove(d, s, n) (bcopy ((s), (d), (n)))
# endif
# include "stb_image_write.h"
#endif  /* HAVE_LIBPNG */

#include <sixel.h>
#include "stdio_stub.h"
#include "compat_stub.h"

#if !defined(O_BINARY) && defined(_O_BINARY)
# define O_BINARY _O_BINARY
#endif  /* !defined(O_BINARY) && !defined(_O_BINARY) */


#if !HAVE_LIBPNG
unsigned char *
stbi_write_png_to_mem(unsigned char *pixels, int stride_bytes,
                      int x, int y, int n, int *out_len);

/*
 * The stb helper lives in another translation unit, so we forward declare it
 * here to satisfy the compiler. The picture below shows the relationship.
 */
unsigned char *
stbi_zlib_compress(unsigned char *data,
                   int data_len,
                   int *out_len,
                   int quality);

/*
 * The CRC generator mirrors the PNG chunk layout so we can reuse the helper
 * from multiple writers.
 *
 *     +------+-------+
 *     | type | data  |
 *     +------+-------+
 */
static unsigned int
png_crc32_update(unsigned int crc, unsigned char const *data,
                 size_t length)
{
    static unsigned int table[256];
    static int initialized;
    unsigned int rem;
    unsigned char octet;
    size_t index;
    int table_index;

    if (!initialized) {
        for (table_index = 0; table_index < 256; ++table_index) {
            rem = (unsigned int)table_index;
            rem <<= 24;
            for (index = 0; index < 8; ++index) {
                if (rem & 0x80000000U) {
                    rem = (rem << 1) ^ 0x04c11db7U;
                } else {
                    rem <<= 1;
                }
            }
            table[table_index] = rem;
        }
        initialized = 1;
    }

    crc = ~crc;
    for (index = 0; index < length; ++index) {
        octet = data[index];
        crc = table[((crc >> 24) ^ octet) & 0xffU] ^ (crc << 8);
    }

    return ~crc;
}

static int
png_write_u32(FILE *output_fp, unsigned int value)
{
    unsigned char buffer[4];
    size_t written;

    buffer[0] = (unsigned char)((value >> 24) & 0xffU);
    buffer[1] = (unsigned char)((value >> 16) & 0xffU);
    buffer[2] = (unsigned char)((value >> 8) & 0xffU);
    buffer[3] = (unsigned char)(value & 0xffU);

    written = fwrite(buffer, 1, sizeof(buffer), output_fp);
    if (written != sizeof(buffer)) {
        return 0;
    }

    return 1;
}

/*
 * Emit a single PNG chunk.
 *     +--------+---------+----------+--------+
 *     | length |  type   |  data    |  CRC   |
 *     +--------+---------+----------+--------+
 */
static int
png_write_chunk(FILE *output_fp, char const *tag,
                unsigned char const *payload, size_t length)
{
    unsigned char type[4];
    unsigned int crc;
    size_t written;

    type[0] = (unsigned char)tag[0];
    type[1] = (unsigned char)tag[1];
    type[2] = (unsigned char)tag[2];
    type[3] = (unsigned char)tag[3];

    if (!png_write_u32(output_fp, (unsigned int)length)) {
        return 0;
    }

    written = fwrite(type, 1, sizeof(type), output_fp);
    if (written != sizeof(type)) {
        return 0;
    }

    if (length > 0) {
        written = fwrite(payload, 1, length, output_fp);
        if (written != length) {
            return 0;
        }
    }

    crc = png_crc32_update(0U, type, sizeof(type));
    if (length > 0) {
        crc = png_crc32_update(crc, payload, length);
    }

    if (!png_write_u32(output_fp, crc)) {
        return 0;
    }

    return 1;
}
#endif  /* !HAVE_LIBPNG */

static SIXELSTATUS
sixel_writer_convert_to_rgba(
    unsigned char       *dst,
    unsigned char const *src,
    int                  src_pixelformat,
    int                  width,
    int                  height)
{
    SIXELSTATUS status = SIXEL_FALSE;
    size_t count = 0u;
    size_t index = 0u;
    unsigned char const *cursor = NULL;
    unsigned char *output = NULL;

    count = (size_t)width * (size_t)height;
    cursor = src;
    output = dst;

    for (index = 0u; index < count; ++index) {
        switch (src_pixelformat) {
        case SIXEL_PIXELFORMAT_RGBA8888:
            output[0] = cursor[0];
            output[1] = cursor[1];
            output[2] = cursor[2];
            output[3] = cursor[3];
            break;
        case SIXEL_PIXELFORMAT_ARGB8888:
            output[0] = cursor[1];
            output[1] = cursor[2];
            output[2] = cursor[3];
            output[3] = cursor[0];
            break;
        case SIXEL_PIXELFORMAT_BGRA8888:
            output[0] = cursor[2];
            output[1] = cursor[1];
            output[2] = cursor[0];
            output[3] = cursor[3];
            break;
        case SIXEL_PIXELFORMAT_ABGR8888:
            output[0] = cursor[3];
            output[1] = cursor[2];
            output[2] = cursor[1];
            output[3] = cursor[0];
            break;
        default:
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        cursor += 4;
        output += 4;
    }

    status = SIXEL_OK;

end:
    return status;
}


static SIXELSTATUS
write_png_to_file(
    unsigned char       /* in */ *data,         /* source pixel data */
    int                 /* in */ width,         /* source data width */
    int                 /* in */ height,        /* source data height */
    unsigned char       /* in */ *palette,      /* palette of source data */
    int                 /* in */ pixelformat,   /* source pixelFormat */
    char const          /* in */ *filename,     /* destination filename */
    sixel_allocator_t   /* in */ *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    FILE *output_fp = NULL;
    unsigned char *pixels = NULL;
    unsigned char *new_pixels = NULL;
    int uses_palette = 0;
    int palette_entries = 0;
    int total_pixels = 0;
    int max_index = 0;
    int i = 0;
    unsigned char *src = NULL;
    unsigned char *dst = NULL;
    int bytes_per_pixel = 3;
#if HAVE_LIBPNG
    int y = 0;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    unsigned char **rows = NULL;
    png_color *png_palette = NULL;
#else
    unsigned char *filtered = NULL;
    unsigned char *compressed = NULL;
    unsigned char *png_data = NULL;
    unsigned char signature[8];
    unsigned char ihdr[13];
    unsigned char *plte = NULL;
    int stride = 0;
    int png_len = 0;
    int write_len = 0;
    size_t payload_size = 0;
#endif  /* HAVE_LIBPNG */

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_PAL4:
        if (palette == NULL) {
            status = SIXEL_BAD_ARGUMENT;
            sixel_helper_set_additional_message(
                "write_png_to_file: no palette is given");
            goto end;
        }
        new_pixels = sixel_allocator_malloc(allocator,
                                            (size_t)(width * height));
        if (new_pixels == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "write_png_to_file: sixel_allocator_malloc() failed");
            goto end;
        }
        pixels = new_pixels;
        status = sixel_helper_normalize_pixelformat(pixels,
                                                    &pixelformat,
                                                    data,
                                                    pixelformat,
                                                    width,
                                                    height);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        break;
    case SIXEL_PIXELFORMAT_PAL8:
        if (palette == NULL) {
            status = SIXEL_BAD_ARGUMENT;
            sixel_helper_set_additional_message(
                "write_png_to_file: no palette is given");
            goto end;
        }
        pixels = data;
        break;
    case SIXEL_PIXELFORMAT_RGB888:
        pixels = data;
        break;
    case SIXEL_PIXELFORMAT_G8:
        src = data;
        dst = pixels = new_pixels
            = sixel_allocator_malloc(allocator, (size_t)(width * height * 3));
        if (new_pixels == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "write_png_to_file: sixel_allocator_malloc() failed");
            goto end;
        }
        if (palette) {
            for (i = 0; i < width * height; ++i, ++src) {
                *dst++ = *(palette + *src * 3 + 0);
                *dst++ = *(palette + *src * 3 + 1);
                *dst++ = *(palette + *src * 3 + 2);
            }
        } else {
            for (i = 0; i < width * height; ++i, ++src) {
                *dst++ = *src;
                *dst++ = *src;
                *dst++ = *src;
            }
        }
        break;
    case SIXEL_PIXELFORMAT_RGB565:
    case SIXEL_PIXELFORMAT_RGB555:
    case SIXEL_PIXELFORMAT_BGR565:
    case SIXEL_PIXELFORMAT_BGR555:
    case SIXEL_PIXELFORMAT_GA88:
    case SIXEL_PIXELFORMAT_AG88:
    case SIXEL_PIXELFORMAT_BGR888:
        pixels = new_pixels = sixel_allocator_malloc(allocator,
                                                    (size_t)(width
                                                             * height
                                                             * 3));
        if (new_pixels == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "write_png_to_file: sixel_allocator_malloc() failed");
            goto end;
        }
        status = sixel_helper_normalize_pixelformat(pixels,
                                                    &pixelformat,
                                                    data,
                                                    pixelformat,
                                                    width, height);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        break;
    case SIXEL_PIXELFORMAT_RGBA8888:
        pixels = data;
        pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
        bytes_per_pixel = 4;
        break;
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
        new_pixels = sixel_allocator_malloc(allocator,
                                            (size_t)width
                                            * (size_t)height
                                            * 4u);
        if (new_pixels == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "write_png_to_file: sixel_allocator_malloc() failed");
            goto end;
        }
        status = sixel_writer_convert_to_rgba(new_pixels,
                                              data,
                                              pixelformat,
                                              width,
                                              height);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        pixels = new_pixels;
        pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
        bytes_per_pixel = 4;
        break;
    default:
        status = SIXEL_BAD_ARGUMENT;
        sixel_helper_set_additional_message(
            "write_png_to_file: unkown pixelformat is specified");
        goto end;
    }

    uses_palette = (pixelformat == SIXEL_PIXELFORMAT_PAL8 && palette != NULL);
    if (uses_palette) {
        total_pixels = width * height;
        max_index = 0;
        for (i = 0; i < total_pixels; ++i) {
            if (pixels[i] > max_index) {
                max_index = pixels[i];
            }
        }
        palette_entries = max_index + 1;
        if (palette_entries < 1) {
            palette_entries = 1;
        }
        if (palette_entries > SIXEL_PALETTE_MAX) {
            palette_entries = SIXEL_PALETTE_MAX;
        }
    }

    if (strcmp(filename, "-") == 0) {
#if defined(O_BINARY)
# if HAVE__SETMODE
        _setmode(STDOUT_FILENO, O_BINARY);
# elif HAVE_SETMODE
        setmode(STDOUT_FILENO, O_BINARY);
# endif  /* HAVE_SETMODE */
#endif  /* defined(O_BINARY) */
        output_fp = stdout;
    } else {
        output_fp = sixel_compat_fopen(filename, "wb");
        if (!output_fp) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message("fopen() failed.");
            goto end;
        }
    }

    /*
     * Palette and RGB output branches fan out here.
     *
     *     +---------------+
     *     | use palette? |
     *     +---------------+
     *        | yes              no |
     *        v                  v
     *   indexed pipeline   RGB pipeline
     */
#if HAVE_LIBPNG
    if (uses_palette) {
        rows = sixel_allocator_malloc(allocator,
                                      (size_t)height
                                      * sizeof(unsigned char *));
        if (rows == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "write_png_to_file: sixel_allocator_malloc() failed");
            goto end;
        }
        for (y = 0; y < height; ++y) {
            rows[y] = pixels + width * y;
        }
    } else {
        rows = sixel_allocator_malloc(allocator,
                                      (size_t)height
                                      * sizeof(unsigned char *));
        if (rows == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "write_png_to_file: sixel_allocator_malloc() failed");
            goto end;
        }
        for (y = 0; y < height; ++y) {
            rows[y] = pixels
                + (size_t)width * (size_t)bytes_per_pixel * (size_t)y;
        }
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                      NULL,
                                      NULL,
                                      NULL);
    if (!png_ptr) {
        status = SIXEL_PNG_ERROR;
        goto end;
    }
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        status = SIXEL_PNG_ERROR;
        goto end;
    }
# if USE_SETJMP && HAVE_SETJMP
    if (setjmp(png_jmpbuf(png_ptr))) {
        status = SIXEL_PNG_ERROR;
        goto end;
    }
# endif
    png_init_io(png_ptr, output_fp);
    if (uses_palette) {
        png_set_IHDR(png_ptr,
                     info_ptr,
                     (png_uint_32)width,
                     (png_uint_32)height,
                     8,
                     PNG_COLOR_TYPE_PALETTE,
                     PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_BASE,
                     PNG_FILTER_TYPE_BASE);
        png_palette = sixel_allocator_malloc(allocator,
                                             (size_t)palette_entries
                                             * sizeof(png_color));
        if (png_palette == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "write_png_to_file: sixel_allocator_malloc() failed");
            goto end;
        }
        for (i = 0; i < palette_entries; ++i) {
            png_palette[i].red = palette[i * 3 + 0];
            png_palette[i].green = palette[i * 3 + 1];
            png_palette[i].blue = palette[i * 3 + 2];
        }
        png_set_PLTE(png_ptr, info_ptr, png_palette, palette_entries);
    } else {
        int color_type;

        color_type = PNG_COLOR_TYPE_RGB;
        if (pixelformat == SIXEL_PIXELFORMAT_RGBA8888) {
            color_type = PNG_COLOR_TYPE_RGBA;
        }
        png_set_IHDR(png_ptr,
                     info_ptr,
                     (png_uint_32)width,
                     (png_uint_32)height,
                     8,
                     color_type,
                     PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_BASE,
                     PNG_FILTER_TYPE_BASE);
    }
    png_write_info(png_ptr, info_ptr);
    png_write_image(png_ptr, rows);
    png_write_end(png_ptr, NULL);
#else
    if (uses_palette) {
        stride = width + 1;
        payload_size = (size_t)stride * (size_t)height;
        filtered = sixel_allocator_malloc(allocator, payload_size);
        if (filtered == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "write_png_to_file: sixel_allocator_malloc() failed");
            goto end;
        }
        for (i = 0; i < height; ++i) {
            filtered[i * stride] = 0;
            memcpy(filtered + i * stride + 1,
                   pixels + i * width,
                   (size_t)width);
        }
        compressed = stbi_zlib_compress(filtered,
                                        (int)payload_size,
                                        &png_len,
                                        stbi_write_png_compression_level);
        if (compressed == NULL) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "stbi_zlib_compress() failed.");
            goto end;
        }
        signature[0] = 0x89;
        signature[1] = 0x50;
        signature[2] = 0x4e;
        signature[3] = 0x47;
        signature[4] = 0x0d;
        signature[5] = 0x0a;
        signature[6] = 0x1a;
        signature[7] = 0x0a;
        write_len = (int)fwrite(signature, 1, sizeof(signature), output_fp);
        if (write_len != (int)sizeof(signature)) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message("fwrite() failed.");
            goto end;
        }
        memset(ihdr, 0, sizeof(ihdr));
        ihdr[0] = (unsigned char)((width >> 24) & 0xff);
        ihdr[1] = (unsigned char)((width >> 16) & 0xff);
        ihdr[2] = (unsigned char)((width >> 8) & 0xff);
        ihdr[3] = (unsigned char)(width & 0xff);
        ihdr[4] = (unsigned char)((height >> 24) & 0xff);
        ihdr[5] = (unsigned char)((height >> 16) & 0xff);
        ihdr[6] = (unsigned char)((height >> 8) & 0xff);
        ihdr[7] = (unsigned char)(height & 0xff);
        ihdr[8] = 8;
        ihdr[9] = 3;
        ihdr[10] = 0;
        ihdr[11] = 0;
        ihdr[12] = 0;
        if (!png_write_chunk(output_fp, "IHDR", ihdr, sizeof(ihdr))) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message("fwrite() failed.");
            goto end;
        }
        plte = sixel_allocator_malloc(allocator,
                                      (size_t)palette_entries * 3U);
        if (plte == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "write_png_to_file: sixel_allocator_malloc() failed");
            goto end;
        }
        for (i = 0; i < palette_entries; ++i) {
            plte[i * 3 + 0] = palette[i * 3 + 0];
            plte[i * 3 + 1] = palette[i * 3 + 1];
            plte[i * 3 + 2] = palette[i * 3 + 2];
        }
        if (!png_write_chunk(output_fp,
                             "PLTE",
                             plte,
                             (size_t)palette_entries * 3U)) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message("fwrite() failed.");
            goto end;
        }
        if (!png_write_chunk(output_fp,
                             "IDAT",
                             compressed,
                             (size_t)png_len)) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message("fwrite() failed.");
            goto end;
        }
        if (!png_write_chunk(output_fp, "IEND", NULL, 0)) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message("fwrite() failed.");
            goto end;
        }
    } else {
        png_data = stbi_write_png_to_mem(pixels,
                                         width * bytes_per_pixel,
                                         width,
                                         height,
                                         bytes_per_pixel,
                                         &png_len);
        if (png_data == NULL) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "stbi_write_png_to_mem() failed.");
            goto end;
        }
        write_len = (int)fwrite(png_data,
                                 1,
                                 (size_t)png_len,
                                 output_fp);
        if (write_len <= 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message("fwrite() failed.");
            goto end;
        }
    }
#endif  /* HAVE_LIBPNG */

    status = SIXEL_OK;

end:
    if (output_fp && output_fp != stdout) {
        fclose(output_fp);
    }
#if HAVE_LIBPNG
    sixel_allocator_free(allocator, rows);
    if (png_ptr) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
    }
    sixel_allocator_free(allocator, png_palette);
#else
    sixel_allocator_free(allocator, compressed);
    sixel_allocator_free(allocator, filtered);
    sixel_allocator_free(allocator, plte);
    sixel_allocator_free(allocator, png_data);
#endif  /* HAVE_LIBPNG */
    sixel_allocator_free(allocator, new_pixels);

    return status;
}


SIXELAPI SIXELSTATUS
sixel_helper_write_image_file(
    unsigned char       /* in */ *data,        /* source pixel data */
    int                 /* in */ width,        /* source data width */
    int                 /* in */ height,       /* source data height */
    unsigned char       /* in */ *palette,     /* palette of source data */
    int                 /* in */ pixelformat,  /* source pixelFormat */
    char const          /* in */ *filename,    /* destination filename */
    int                 /* in */ imageformat,  /* destination imageformat */
    sixel_allocator_t   /* in */ *allocator)   /* allocator object */
{
    SIXELSTATUS status = SIXEL_FALSE;

    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        sixel_allocator_ref(allocator);
    }

    if (width > SIXEL_WIDTH_LIMIT) {
        sixel_helper_set_additional_message(
            "sixel_encode: bad width parameter."
            " (width > SIXEL_WIDTH_LIMIT)");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    if (width > SIXEL_HEIGHT_LIMIT) {
        sixel_helper_set_additional_message(
            "sixel_encode: bad width parameter."
            " (width > SIXEL_HEIGHT_LIMIT)");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    if (height < 1) {
        sixel_helper_set_additional_message(
            "sixel_encode: bad height parameter."
            " (height < 1)");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    if (width < 1) {
        sixel_helper_set_additional_message(
            "sixel_encode: bad width parameter."
            " (width < 1)");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    if (height < 1) {
        sixel_helper_set_additional_message(
            "sixel_encode: bad height parameter."
            " (height < 1)");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    switch (imageformat) {
    case SIXEL_FORMAT_PNG:
        status = write_png_to_file(data, width, height, palette,
                                   pixelformat, filename, allocator);
        break;
    case SIXEL_FORMAT_GIF:
    case SIXEL_FORMAT_BMP:
    case SIXEL_FORMAT_JPG:
    case SIXEL_FORMAT_TGA:
    case SIXEL_FORMAT_WBMP:
    case SIXEL_FORMAT_TIFF:
    case SIXEL_FORMAT_SIXEL:
    case SIXEL_FORMAT_PNM:
    case SIXEL_FORMAT_GD2:
    case SIXEL_FORMAT_PSD:
    case SIXEL_FORMAT_HDR:
    default:
        status = SIXEL_NOT_IMPLEMENTED;
        goto end;
        break;
    }

end:
    sixel_allocator_unref(allocator);
    return status;
}


#if HAVE_TESTS
static int
test1(void)
{
    int nret = EXIT_FAILURE;
    SIXELSTATUS status;
    unsigned char pixels[] = {0xff, 0xff, 0xff};

    status = sixel_helper_write_image_file(
        pixels,
        1,
        1,
        NULL,
        SIXEL_PIXELFORMAT_RGB888,
        "output.gif",
        SIXEL_FORMAT_GIF,
        NULL);

    if (!SIXEL_FAILED(status)) {
        goto error;
    }
    nret = EXIT_SUCCESS;

error:
    return nret;
}


static int
test2(void)
{
    int nret = EXIT_FAILURE;
    SIXELSTATUS status;
    unsigned char pixels[] = {0xff, 0xff, 0xff};

    status = sixel_helper_write_image_file(
        pixels,
        1,
        1,
        NULL,
        SIXEL_PIXELFORMAT_RGB888,
        "test-output.png",
        SIXEL_FORMAT_PNG,
        NULL);

    if (SIXEL_FAILED(status)) {
        goto error;
    }
    nret = EXIT_SUCCESS;

error:
    return nret;
}


static int
test3(void)
{
    int nret = EXIT_FAILURE;
    SIXELSTATUS status;
    unsigned char pixels[] = {0x00, 0x7f, 0xff};
    sixel_dither_t *dither = sixel_dither_get(SIXEL_BUILTIN_G8);

    status = sixel_helper_write_image_file(
        pixels,
        1,
        1,
        NULL,
        SIXEL_PIXELFORMAT_G8,
        "test-output.png",
        SIXEL_FORMAT_PNG,
        NULL);

    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_helper_write_image_file(
        pixels,
        1,
        1,
        sixel_dither_get_palette(dither),
        SIXEL_PIXELFORMAT_G8,
        "test-output.png",
        SIXEL_FORMAT_PNG,
        NULL);

    if (SIXEL_FAILED(status)) {
        goto error;
    }
    nret = EXIT_SUCCESS;

error:
    return nret;
}


static int
test4(void)
{
    int nret = EXIT_FAILURE;
    SIXELSTATUS status;
    unsigned char pixels[] = {0xa0};
    sixel_dither_t *dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256);

    status = sixel_helper_write_image_file(
        pixels,
        1,
        1,
        sixel_dither_get_palette(dither),
        SIXEL_PIXELFORMAT_PAL1,
        "test-output.png",
        SIXEL_FORMAT_PNG,
        NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_helper_write_image_file(
        pixels,
        1,
        1,
        NULL,
        SIXEL_PIXELFORMAT_PAL1,
        "test-output.png",
        SIXEL_FORMAT_PNG,
        NULL);
    if (status != SIXEL_BAD_ARGUMENT) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


static int
test5(void)
{
    int nret = EXIT_FAILURE;
    SIXELSTATUS status;
    unsigned char pixels[] = {0x00};
    sixel_dither_t *dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256);

    status = sixel_helper_write_image_file(
        pixels,
        1,
        1,
        sixel_dither_get_palette(dither),
        SIXEL_PIXELFORMAT_PAL8,
        "test-output.png",
        SIXEL_FORMAT_PNG,
        NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_helper_write_image_file(
        pixels,
        1,
        1,
        NULL,
        SIXEL_PIXELFORMAT_PAL8,
        "test-output.png",
        SIXEL_FORMAT_PNG,
        NULL);
    if (status != SIXEL_BAD_ARGUMENT) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


static int
test6(void)
{
    int nret = EXIT_FAILURE;
    SIXELSTATUS status;
    unsigned char pixels[] = {0x00, 0x7f, 0xff};

    status = sixel_helper_write_image_file(
        pixels,
        1,
        1,
        NULL,
        SIXEL_PIXELFORMAT_BGR888,
        "test-output.png",
        SIXEL_FORMAT_PNG,
        NULL);

    if (SIXEL_FAILED(status)) {
        goto error;
    }
    nret = EXIT_SUCCESS;

error:
    return nret;
}


SIXELAPI int
sixel_writer_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (* testcase)(void);

    static testcase const testcases[] = {
        test1,
        test2,
        test3,
        test4,
        test5,
        test6,
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
