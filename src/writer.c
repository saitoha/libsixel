/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2016 Hayaki Saito
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

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

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
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif  /* HAVE_FCNTL_H */
#include "compat_stub.h"
#if HAVE_LIBPNG
# include <png.h>
#endif
#if HAVE_LIBPNG && HAVE_SETJMP && HAVE_LONGJMP && defined(PNG_SETJMP_SUPPORTED)
# define SIXEL_WRITER_USE_LIBPNG 1
#else
# define SIXEL_WRITER_USE_LIBPNG 0
/* Use the bundled writer when libpng errors cannot be caught. */
#endif  /* SIXEL_WRITER_USE_LIBPNG */

#include <sixel.h>
#include "stdio_stub.h"
#include "timeline-logger.h"
#include "writer.h"

/* A private test copy can substitute failing writer operations without
 * adding runtime hooks to the normal writer. */
#ifndef SIXEL_PNG_IO_OPEN
# define SIXEL_PNG_IO_OPEN sixel_compat_fopen
# define SIXEL_PNG_IO_WRITE fwrite
# define SIXEL_PNG_IO_FLUSH fflush
# define SIXEL_PNG_IO_CLOSE fclose
# define SIXEL_PNG_IO_FREE free
# define SIXEL_PNG_IO_CREATE_WRITE png_create_write_struct
# define SIXEL_PNG_IO_CREATE_INFO png_create_info_struct
# define SIXEL_PNG_IO_DESTROY png_destroy_write_struct
#endif

#if !defined(O_BINARY) && defined(_O_BINARY)
# define O_BINARY _O_BINARY
#endif  /* !defined(O_BINARY) && !defined(_O_BINARY) */


#if SIXEL_WRITER_USE_LIBPNG
/* Track whether the current libpng call supplied its own diagnostic. */
static void
sixel_writer_png_record_message(png_structp png_ptr,
                                png_const_charp message)
{
    int *reported;

    if (message != NULL) {
        sixel_helper_set_additional_message(message);
        reported = png_get_error_ptr(png_ptr);
        if (reported != NULL) {
            *reported = 1;
        }
    }
}


/*
 * Capture libpng write errors so callers receive the library detail string
 * instead of only the generic SIXEL_PNG_ERROR status.
 */
static void
sixel_writer_png_error_callback(png_structp png_ptr,
                                png_const_charp error_message)
{
    sixel_writer_png_record_message(png_ptr, error_message);
    longjmp(png_jmpbuf(png_ptr), 1);
}


/*
 * Keep warnings visible in diagnostics without interrupting the write path.
 */
static void
sixel_writer_png_warning_callback(png_structp png_ptr,
                                  png_const_charp warning_message)
{
    sixel_writer_png_record_message(png_ptr, warning_message);
}
/*
 * Keep write callback state in a tiny context so timeline logs can identify
 * how many chunks reached fwrite before libpng reports an error.
 */
typedef struct sixel_writer_png_io_context {
    FILE *output_fp;
    sixel_timeline_logger_t *logger;
    int logger_prepared;
    size_t write_calls;
    size_t write_bytes;
} sixel_writer_png_io_context_t;


static void
sixel_writer_png_write_callback(png_structp png_ptr,
                                png_bytep data,
                                png_size_t length)
{
    sixel_writer_png_io_context_t *ctx;
    size_t written;

    ctx = (sixel_writer_png_io_context_t *)png_get_io_ptr(png_ptr);
    if (ctx == NULL || ctx->output_fp == NULL) {
        png_error(png_ptr, "writer io context is unavailable");
        return;
    }

    if (ctx->logger_prepared) {
        sixel_timeline_logger_logf(ctx->logger,
                          "io",
                          "png",
                          "libpng_stream_write_begin",
                          0,
                          "call=%lu bytes=%lu",
                          (unsigned long)(ctx->write_calls + 1u),
                          (unsigned long)length);
    }

    written = SIXEL_PNG_IO_WRITE(data, 1, length, ctx->output_fp);
    if (written != (size_t)length) {
        if (ctx->logger_prepared) {
            sixel_timeline_logger_logf(ctx->logger,
                              "io",
                              "png",
                              "libpng_stream_write_failed",
                              0,
                              "call=%lu requested=%lu written=%lu errno=%d",
                              (unsigned long)(ctx->write_calls + 1u),
                              (unsigned long)length,
                              (unsigned long)written,
                              errno);
        }
        png_error(png_ptr, "fwrite() failed during PNG output");
        return;
    }

    ctx->write_calls += 1u;
    ctx->write_bytes += written;
    if (ctx->logger_prepared) {
        sixel_timeline_logger_logf(ctx->logger,
                          "io",
                          "png",
                          "libpng_stream_write_done",
                          0,
                          "call=%lu total_bytes=%lu",
                          (unsigned long)ctx->write_calls,
                          (unsigned long)ctx->write_bytes);
    }
}


static void
sixel_writer_png_flush_callback(png_structp png_ptr)
{
    sixel_writer_png_io_context_t *ctx;

    ctx = (sixel_writer_png_io_context_t *)png_get_io_ptr(png_ptr);
    if (ctx == NULL || ctx->output_fp == NULL) {
        png_error(png_ptr, "writer io context is unavailable on flush");
        return;
    }

    if (ctx->logger_prepared) {
        sixel_timeline_logger_logf(ctx->logger,
                          "io",
                          "png",
                          "libpng_stream_flush_begin",
                          0,
                          "call=%lu bytes=%lu",
                          (unsigned long)ctx->write_calls,
                          (unsigned long)ctx->write_bytes);
    }

    if (SIXEL_PNG_IO_FLUSH(ctx->output_fp) != 0) {
        if (ctx->logger_prepared) {
            sixel_timeline_logger_logf(ctx->logger,
                              "io",
                              "png",
                              "libpng_stream_flush_failed",
                              0,
                              "errno=%d",
                              errno);
        }
        png_error(png_ptr, "fflush() failed during PNG output");
        return;
    }

    if (ctx->logger_prepared) {
        sixel_timeline_logger_logf(ctx->logger,
                          "io",
                          "png",
                          "libpng_stream_flush_done",
                          0,
                          "call=%lu bytes=%lu",
                          (unsigned long)ctx->write_calls,
                          (unsigned long)ctx->write_bytes);
    }
}

typedef struct sixel_writer_png_image {
    png_bytepp rows;
    png_colorp palette;
    int palette_entries;
    png_uint_32 width;
    png_uint_32 height;
    int pixelformat;
    int srgb;
    unsigned char const *icc;
    png_uint_32 icc_length;
} sixel_writer_png_image_t;


/* The caller owns cleanup. Keep the jump in this small frame so none
 * of the caller's resource owners can be clobbered by libpng errors. */
static SIXELSTATUS
sixel_writer_png_emit(png_structp png_ptr, png_infop info_ptr,
                      sixel_writer_png_io_context_t *io,
                      sixel_writer_png_image_t const *image)
{
    int color_type;

    if (setjmp(png_jmpbuf(png_ptr))) {
        if (io->logger_prepared) {
            sixel_timeline_logger_logf(io->logger,
                              "io",
                              "png",
                              "libpng_error_callback",
                              0,
                              "status=%d",
                              SIXEL_PNG_ERROR);
        }
        return SIXEL_PNG_ERROR;
    }
    png_set_write_fn(png_ptr,
                     io,
                     &sixel_writer_png_write_callback,
                     &sixel_writer_png_flush_callback);
    if (image->palette != NULL) {
        png_set_IHDR(png_ptr,
                     info_ptr,
                     image->width,
                     image->height,
                     8,
                     PNG_COLOR_TYPE_PALETTE,
                     PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_BASE,
                     PNG_FILTER_TYPE_BASE);
        png_set_PLTE(png_ptr, info_ptr, image->palette,
                      image->palette_entries);
    } else {
        color_type = PNG_COLOR_TYPE_RGB;
        if (image->pixelformat == SIXEL_PIXELFORMAT_RGBA8888) {
            color_type = PNG_COLOR_TYPE_RGBA;
        }
        png_set_IHDR(png_ptr,
                     info_ptr,
                     image->width,
                     image->height,
                     8,
                     color_type,
                     PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_BASE,
                     PNG_FILTER_TYPE_BASE);
    }
    if (io->logger_prepared) {
        sixel_timeline_logger_logf(io->logger,
                          "io",
                          "png",
                          "libpng_write_info_begin",
                          0);
    }
    /* Metadata describes the supplied encoded values, including PLTE. */
    if (image->srgb) {
        png_set_sRGB(png_ptr, info_ptr, PNG_sRGB_INTENT_RELATIVE);
    } else if (image->icc != NULL) {
        png_set_iCCP(png_ptr, info_ptr, "RGB", PNG_COMPRESSION_TYPE_BASE,
                     image->icc, image->icc_length);
    }
    png_write_info(png_ptr, info_ptr);
    if (io->logger_prepared) {
        sixel_timeline_logger_logf(io->logger,
                          "io",
                          "png",
                          "libpng_write_info_done",
                          0);
        sixel_timeline_logger_logf(io->logger,
                          "io",
                          "png",
                          "libpng_write_image_begin",
                          0);
    }
    png_write_image(png_ptr, image->rows);
    if (io->logger_prepared) {
        sixel_timeline_logger_logf(io->logger,
                          "io",
                          "png",
                          "libpng_write_image_done",
                          0);
        sixel_timeline_logger_logf(io->logger,
                          "io",
                          "png",
                          "libpng_write_end_begin",
                          0);
    }
    png_write_end(png_ptr, NULL);
    if (io->logger_prepared) {
        sixel_timeline_logger_logf(io->logger,
                          "io",
                          "png",
                          "libpng_write_end_done",
                          0);
    }

    return SIXEL_OK;
}
#endif  /* SIXEL_WRITER_USE_LIBPNG */


#if !SIXEL_WRITER_USE_LIBPNG
/* The bundled encoder and its allocator live in stb_image_write.c. */
extern int stbi_write_png_compression_level;

unsigned char *
stbi_write_png_to_mem(const unsigned char *pixels, int stride_bytes,
                      int x, int y, int n, int *out_len);

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
    unsigned char octet;
    size_t index;
    int bit;

    /*
     * PNG uses the reflected form of CRC-32.  Bytes enter at the low end and
     * every bit shifts right against the reversed 0xedb88320 polynomial.
     * Keeping the complement at this function boundary allows callers to
     * extend one chunk CRC across its type and payload in separate calls.
     */
    crc = ~crc;
    for (index = 0; index < length; ++index) {
        octet = data[index];
        crc ^= (unsigned int)octet;
        for (bit = 0; bit < 8; ++bit) {
            if (crc & 1U) {
                crc = (crc >> 1) ^ 0xedb88320U;
            } else {
                crc >>= 1;
            }
        }
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

    written = SIXEL_PNG_IO_WRITE(buffer, 1, sizeof(buffer), output_fp);
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
sixel_builtin_png_write_chunk(FILE *output_fp, char const *tag,
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

    written = SIXEL_PNG_IO_WRITE(type, 1, sizeof(type), output_fp);
    if (written != sizeof(type)) {
        return 0;
    }

    if (length > 0) {
        written = SIXEL_PNG_IO_WRITE(payload, 1, length, output_fp);
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

/* Both builtin pixel paths insert color information immediately after
 * IHDR. Keep sRGB and iCCP interpretations mutually exclusive. */
static int
png_write_color(FILE *fp, int srgb, unsigned char const *icc, size_t length)
{
    static unsigned char const intent = 1u;

    if (srgb) {
        return sixel_builtin_png_write_chunk(fp, "sRGB", &intent, 1u);
    }
    return icc == NULL ||
           sixel_builtin_png_write_chunk(fp, "iCCP", icc, length);
}
#endif  /* !SIXEL_WRITER_USE_LIBPNG */

/* The caller dispatches packed layouts that need conversion. */
static SIXELSTATUS
sixel_writer_convert_to_rgba(
    unsigned char       *dst,
    unsigned char const *src,
    int                  src_pixelformat,
    int                  width,
    int                  height)
{
    SIXELSTATUS status;
    size_t count;
    size_t index;
    unsigned char const *cursor;
    unsigned char *output;

    count = (size_t)width * (size_t)height;
    cursor = src;
    output = dst;

    for (index = 0u; index < count; ++index) {
        switch (src_pixelformat) {
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
        case SIXEL_PIXELFORMAT_XRGB8888:
            output[0] = cursor[1];
            output[1] = cursor[2];
            output[2] = cursor[3];
            output[3] = 0xffU;
            break;
        case SIXEL_PIXELFORMAT_RGBX8888:
            output[0] = cursor[0];
            output[1] = cursor[1];
            output[2] = cursor[2];
            output[3] = 0xffU;
            break;
        case SIXEL_PIXELFORMAT_XBGR8888:
            output[0] = cursor[3];
            output[1] = cursor[2];
            output[2] = cursor[1];
            output[3] = 0xffU;
            break;
        case SIXEL_PIXELFORMAT_BGRX8888:
            output[0] = cursor[2];
            output[1] = cursor[1];
            output[2] = cursor[0];
            output[3] = 0xffU;
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
    FILE *output_stream,
    int srgb, unsigned char const *icc, size_t icc_length,
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
    sixel_timeline_logger_t *logger;
    int logger_prepared = 0;
#if SIXEL_WRITER_USE_LIBPNG
    int y = 0;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    unsigned char **rows = NULL;
    png_color *png_palette = NULL;
    int libpng_message_reported;
    sixel_writer_png_io_context_t png_io_ctx;
#else
    unsigned char *filtered = NULL;
    unsigned char *compressed = NULL;
    unsigned char *png_data = NULL;
    unsigned char signature[8];
    unsigned char ihdr[13];
    unsigned char *plte = NULL;
    int stride = 0;
    int png_len = 0;
    unsigned char *icc_compressed = NULL;
    unsigned char *icc_chunk = NULL;
    int icc_compressed_length = 0;
    size_t icc_chunk_length = 0u;
    size_t payload_size = 0;
#endif  /* SIXEL_WRITER_USE_LIBPNG */

    logger = NULL;
    (void)sixel_timeline_logger_prepare_env(allocator, &logger);
    logger_prepared = logger != NULL;
    if (logger_prepared) {
        sixel_timeline_logger_logf(logger,
                          "io",
                          "png",
                          "write_begin",
                          0,
                          "target=%s width=%d height=%d fmt=%d",
                          filename != NULL ? filename : "(null)",
                          width,
                          height,
                          pixelformat);
    }
#if SIXEL_WRITER_USE_LIBPNG
    png_io_ctx.output_fp = NULL;
    png_io_ctx.logger = logger;
    png_io_ctx.logger_prepared = logger_prepared;
    png_io_ctx.write_calls = 0u;
    png_io_ctx.write_bytes = 0u;
#endif  /* SIXEL_WRITER_USE_LIBPNG */

#if !SIXEL_WRITER_USE_LIBPNG
    if (icc != NULL) {
        icc_compressed = stbi_zlib_compress((unsigned char *)icc,
            (int)icc_length, &icc_compressed_length, 8);
        if (icc_compressed == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "PNG ICC profile compression failed.");
            goto end;
        }
        icc_chunk_length = (size_t)icc_compressed_length + 5u;
        icc_chunk = sixel_allocator_malloc(allocator, icc_chunk_length);
        if (icc_chunk == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "PNG iCCP chunk allocation failed.");
            goto end;
        }
        memcpy(icc_chunk, "RGB\0\0", 5u);
        memcpy(icc_chunk + 5u, icc_compressed,
               (size_t)icc_compressed_length);
    }
#endif

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
    case SIXEL_PIXELFORMAT_RGBFLOAT32:
    case SIXEL_PIXELFORMAT_LINEARRGBFLOAT32:
    case SIXEL_PIXELFORMAT_OKLABFLOAT32:
    case SIXEL_PIXELFORMAT_CIELABFLOAT32:
    case SIXEL_PIXELFORMAT_DIN99DFLOAT32:
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
    case SIXEL_PIXELFORMAT_XRGB8888:
    case SIXEL_PIXELFORMAT_RGBX8888:
    case SIXEL_PIXELFORMAT_XBGR8888:
    case SIXEL_PIXELFORMAT_BGRX8888:
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
            "write_png_to_file: unknown pixelformat is specified");
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
        /* The expanded byte indices bound the entry count to 1 through 256. */
        palette_entries = max_index + 1;
    }

    if (logger_prepared) {
        sixel_timeline_logger_logf(logger,
                          "io",
                          "png",
                          "file_open_begin",
                          0,
                          "target=%s",
                          filename != NULL ? filename : "(null)");
    }
    if (output_stream != NULL) {
        output_fp = output_stream;
    } else if (filename == NULL) {
        /* Format normalization and logging may already own allocations. */
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    } else if (strcmp(filename, "-") == 0) {
#if defined(O_BINARY)
        (void)sixel_compat_set_binary(STDOUT_FILENO);
#endif  /* defined(O_BINARY) */
        output_fp = stdout;
    } else {
        output_fp = SIXEL_PNG_IO_OPEN(filename, "wb");
        if (!output_fp) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            if (logger_prepared) {
                sixel_timeline_logger_logf(logger,
                                  "io",
                                  "png",
                                  "file_open_failed",
                                  0,
                                  "status=%d",
                                  status);
            }
            sixel_helper_set_additional_message("fopen() failed.");
            goto end;
        }
    }
    if (logger_prepared) {
        sixel_timeline_logger_logf(logger,
                          "io",
                          "png",
                          "file_open_done",
                          0,
                          "target=%s",
                          filename != NULL ? filename : "(null)");
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
#if SIXEL_WRITER_USE_LIBPNG
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
        /* Cleanup needs this pointer even after a libpng longjmp. Keep
         * it stable by completing the allocation before setjmp. */
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

    if (logger_prepared) {
        sixel_timeline_logger_logf(logger,
                          "io",
                          "png",
                          "libpng_create_begin",
                          0);
    }
    libpng_message_reported = 0;
    png_ptr = SIXEL_PNG_IO_CREATE_WRITE(PNG_LIBPNG_VER_STRING,
                                        &libpng_message_reported,
                                        &sixel_writer_png_error_callback,
                                        &sixel_writer_png_warning_callback);
    if (!png_ptr) {
        status = SIXEL_PNG_ERROR;
        if (!libpng_message_reported) {
            sixel_helper_set_additional_message(
                "png_create_write_struct() failed.");
        }
        if (logger_prepared) {
            sixel_timeline_logger_logf(logger,
                              "io",
                              "png",
                              "libpng_create_failed",
                              0,
                              "status=%d",
                              status);
        }
        goto end;
    }
    if (logger_prepared) {
        sixel_timeline_logger_logf(logger,
                          "io",
                          "png",
                          "libpng_create_done",
                          0);
    }
    libpng_message_reported = 0;
    info_ptr = SIXEL_PNG_IO_CREATE_INFO(png_ptr);
    if (!info_ptr) {
        status = SIXEL_PNG_ERROR;
        if (!libpng_message_reported) {
            sixel_helper_set_additional_message(
                "png_create_info_struct() failed.");
        }
        if (logger_prepared) {
            sixel_timeline_logger_logf(logger,
                              "io",
                              "png",
                              "libpng_info_failed",
                              0,
                              "status=%d",
                              status);
        }
        goto end;
    }
    if (logger_prepared) {
        sixel_timeline_logger_logf(logger,
                          "io",
                          "png",
                          "libpng_info_done",
                          0);
    }
    png_io_ctx.output_fp = output_fp;
    status = sixel_writer_png_emit(
        png_ptr, info_ptr, &png_io_ctx,
        &(sixel_writer_png_image_t){
            rows, png_palette, palette_entries,
            (png_uint_32)width, (png_uint_32)height,
            pixelformat, srgb, icc, (png_uint_32)icc_length
        });
    if (SIXEL_FAILED(status)) {
        goto end;
    }
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
        if (SIXEL_PNG_IO_WRITE(signature, 1u, sizeof(signature), output_fp) !=
            sizeof(signature)) {
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
        if (!sixel_builtin_png_write_chunk(output_fp, "IHDR",
                                           ihdr, sizeof(ihdr))) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message("fwrite() failed.");
            goto end;
        }
        if (!png_write_color(output_fp, srgb, icc_chunk,
                             icc_chunk_length)) {
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
        if (!sixel_builtin_png_write_chunk(output_fp, "PLTE", plte,
                                           (size_t)palette_entries * 3U)) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message("fwrite() failed.");
            goto end;
        }
        if (!sixel_builtin_png_write_chunk(output_fp, "IDAT", compressed,
                                           (size_t)png_len)) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message("fwrite() failed.");
            goto end;
        }
        if (!sixel_builtin_png_write_chunk(output_fp, "IEND", NULL, 0)) {
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
        /* stb owns image compression; insert metadata after its IHDR
         * without decoding, recoloring, or recompressing any pixels. */
        if (png_len < 33 ||
            SIXEL_PNG_IO_WRITE(png_data, 1u, 33u, output_fp) != 33u ||
            !png_write_color(output_fp, srgb, icc_chunk,
                             icc_chunk_length) ||
            SIXEL_PNG_IO_WRITE(png_data + 33u, 1u, (size_t)png_len - 33u,
                   output_fp) != (size_t)png_len - 33u) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message("fwrite() failed.");
            goto end;
        }
    }
#endif  /* SIXEL_WRITER_USE_LIBPNG */

    /*
     * Emscripten's Node stdout path does not reliably drain the C stdio
     * buffer at process exit.  Flush explicitly so stdout PNG streams keep the
     * final chunk bytes even when the destination is not closed here.
     */
    if (SIXEL_PNG_IO_FLUSH(output_fp) != 0) {
        status = (SIXEL_LIBC_ERROR | (errno & 0xff));
        sixel_helper_set_additional_message("fflush() failed.");
        goto end;
    }

    status = SIXEL_OK;

end:
    if (output_fp && output_fp != stdout && output_fp != output_stream) {
        if (logger_prepared) {
            sixel_timeline_logger_logf(logger,
                              "io",
                              "png",
                              "file_close_begin",
                              0,
                              "target=%s",
                              filename != NULL ? filename : "(null)");
        }
        /* A close error can fail an otherwise complete write, but must
         * preserve the status and diagnostic of an earlier failure. */
        if (SIXEL_PNG_IO_CLOSE(output_fp) != 0 && SIXEL_SUCCEEDED(status)) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message("fclose() failed.");
        }
        if (logger_prepared) {
            sixel_timeline_logger_logf(logger,
                              "io",
                              "png",
                              "file_close_done",
                              0,
                              "target=%s",
                              filename != NULL ? filename : "(null)");
        }
    }
#if SIXEL_WRITER_USE_LIBPNG
    sixel_allocator_free(allocator, rows);
    if (png_ptr) {
        SIXEL_PNG_IO_DESTROY(&png_ptr, &info_ptr);
    }
    sixel_allocator_free(allocator, png_palette);
#else
    /* stb allocates these buffers with the C runtime. */
    SIXEL_PNG_IO_FREE(icc_compressed);
    SIXEL_PNG_IO_FREE(compressed);
    SIXEL_PNG_IO_FREE(png_data);
    sixel_allocator_free(allocator, icc_chunk);
    sixel_allocator_free(allocator, filtered);
    sixel_allocator_free(allocator, plte);
#endif  /* SIXEL_WRITER_USE_LIBPNG */
    sixel_allocator_free(allocator, new_pixels);
    if (logger_prepared) {
        sixel_timeline_logger_logf(logger,
                          "io",
                          "png",
                          SIXEL_FAILED(status)
                              ? "write_failed"
                              : "write_done",
                          0,
                          "status=%d",
                          status);
    }
    sixel_timeline_logger_unref(logger);

    return status;
}

#undef SIXEL_WRITER_USE_LIBPNG
#undef SIXEL_PNG_IO_OPEN
#undef SIXEL_PNG_IO_WRITE
#undef SIXEL_PNG_IO_FLUSH
#undef SIXEL_PNG_IO_CLOSE
#undef SIXEL_PNG_IO_FREE
#undef SIXEL_PNG_IO_CREATE_WRITE
#undef SIXEL_PNG_IO_CREATE_INFO
#undef SIXEL_PNG_IO_DESTROY


SIXEL_INTERNAL_API SIXELSTATUS
sixel_write_image_color(
    unsigned char       /* in */ *data,        /* source pixel data */
    int                 /* in */ width,        /* source data width */
    int                 /* in */ height,       /* source data height */
    unsigned char       /* in */ *palette,     /* palette of source data */
    int                 /* in */ pixelformat,  /* source pixelFormat */
    char const          /* in */ *filename,    /* destination filename */
    FILE *output_stream,
    int                 /* in */ imageformat,  /* destination imageformat */
    sixel_writer_color_t const *color,
    sixel_allocator_t   /* in */ *allocator)   /* allocator object */
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char *icc = NULL;
    size_t icc_length = 0u;

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

    if (height > SIXEL_HEIGHT_LIMIT) {
        sixel_helper_set_additional_message(
            "sixel_encode: bad height parameter."
            " (height > SIXEL_HEIGHT_LIMIT)");
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

    if (color != NULL) {
        if ((color->srgb && color->definition != NULL) ||
            (!color->srgb && color->definition == NULL) ||
            (pixelformat != SIXEL_PIXELFORMAT_PAL8 &&
             pixelformat != SIXEL_PIXELFORMAT_RGB888 &&
             pixelformat != SIXEL_PIXELFORMAT_RGBA8888)) {
            sixel_helper_set_additional_message(
                "color-aware PNG output requires encoded 8-bit RGB values.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (color->definition != NULL) {
            status = sixel_cdef_export_icc(color->definition, &icc,
                                          &icc_length, allocator);
            if (SIXEL_FAILED(status)) {
                if (status == SIXEL_BAD_ALLOCATION) {
                    sixel_helper_set_additional_message(
                        "PNG ICC profile allocation failed.");
                }
                goto end;
            }
        }
    }

    switch (imageformat) {
    case SIXEL_FORMAT_PNG:
        status = write_png_to_file(data, width, height, palette,
                                   pixelformat, filename, output_stream,
                                   color != NULL && color->srgb,
                                   icc, icc_length, allocator);
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
    if (icc != NULL) {
        sixel_allocator_free(allocator, icc);
    }
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

/* Legacy callers have not specified an interpretation. Preserve their
 * untagged file contract rather than guessing from the pixel layout. */
SIXELAPI SIXELSTATUS
sixel_helper_write_image_file(unsigned char *data, int width, int height,
                               unsigned char *palette, int pixelformat,
                               char const *filename, int imageformat,
                               sixel_allocator_t *allocator)
{
    return sixel_write_image_color(data, width, height, palette,
                                    pixelformat, filename, NULL, imageformat,
                                    NULL, allocator);
}
