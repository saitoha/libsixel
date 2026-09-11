/* SPDX-License-Identifier: MIT */

#include "loader_builtin_gif_test_common.h"

unsigned char const edge_palette_rgb[12] = {
    0x00u, 0x00u, 0x00u,
    0xffu, 0x00u, 0x00u,
    0x00u, 0xffu, 0x00u,
    0x00u, 0x00u, 0xffu
};

static void
edge_lzw_code(unsigned char *compressed,
              size_t capacity,
              size_t *length,
              unsigned int *bits,
              int *valid_bits,
              unsigned int code,
              int code_width,
              int *failed)
{
    if (compressed == NULL || length == NULL || bits == NULL ||
        valid_bits == NULL || failed == NULL || *failed != 0) {
        return;
    }

    *bits |= code << *valid_bits;
    *valid_bits += code_width;
    while (*valid_bits >= 8) {
        if (*length >= capacity) {
            *failed = 1;
            return;
        }
        compressed[(*length)++] = (unsigned char)(*bits & 0xffu);
        *bits >>= 8;
        *valid_bits -= 8;
    }
}

void
edge_gif_begin(edge_writer_t *writer,
               int version_89,
               unsigned int width,
               unsigned int height)
{
    static unsigned char const signature_87[6] = {
        'G', 'I', 'F', '8', '7', 'a'
    };
    static unsigned char const signature_89[6] = {
        'G', 'I', 'F', '8', '9', 'a'
    };

    edge_put_bytes(writer,
                   version_89 != 0 ? signature_89 : signature_87,
                   sizeof(signature_87));
    edge_put_u16le(writer, width);
    edge_put_u16le(writer, height);
    edge_put_u8(writer, 0x81u);
    edge_put_u8(writer, 0u);
    edge_put_u8(writer, 0u);
    edge_put_bytes(writer, edge_palette_rgb, sizeof(edge_palette_rgb));
}

/*
 * Literal-only LZW is intentionally simple but still grows the dictionary.
 * With enough pixels it crosses every code-width boundary through 12 bits.
 * clear_each keeps hand-built small images independent of dictionary state.
 */
static void
edge_gif_image_core(edge_writer_t *writer,
                    unsigned int x,
                    unsigned int y,
                    unsigned int width,
                    unsigned int height,
                    int interlaced,
                    unsigned char const *local_palette,
                    unsigned char const *pixels,
                    size_t pixel_count,
                    int clear_each)
{
    unsigned char compressed[8192];
    size_t compressed_length;
    size_t pixel_index;
    size_t block_offset;
    size_t block_size;
    unsigned int bits;
    int valid_bits;
    int code_width;
    int code_mask;
    int available;
    int have_old_code;
    int failed;

    compressed_length = 0u;
    pixel_index = 0u;
    block_offset = 0u;
    block_size = 0u;
    bits = 0u;
    valid_bits = 0;
    code_width = 3;
    code_mask = 7;
    available = 6;
    have_old_code = 0;
    failed = 0;
    if (writer == NULL || pixels == NULL ||
        pixel_count != (size_t)width * (size_t)height) {
        if (writer != NULL) {
            writer->failed = 1;
        }
        return;
    }

    edge_put_u8(writer, 0x2cu);
    edge_put_u16le(writer, x);
    edge_put_u16le(writer, y);
    edge_put_u16le(writer, width);
    edge_put_u16le(writer, height);
    edge_put_u8(writer,
                (interlaced != 0 ? 0x40u : 0u) |
                (local_palette != NULL ? 0x81u : 0u));
    if (local_palette != NULL) {
        edge_put_bytes(writer, local_palette, sizeof(edge_palette_rgb));
    }
    edge_put_u8(writer, 2u);

    edge_lzw_code(compressed,
                  sizeof(compressed),
                  &compressed_length,
                  &bits,
                  &valid_bits,
                  4u,
                  code_width,
                  &failed);
    for (pixel_index = 0u; pixel_index < pixel_count; ++pixel_index) {
        edge_lzw_code(compressed,
                      sizeof(compressed),
                      &compressed_length,
                      &bits,
                      &valid_bits,
                      pixels[pixel_index],
                      code_width,
                      &failed);
        if (have_old_code != 0) {
            ++available;
            if ((available & code_mask) == 0 && available <= 0x0fff) {
                ++code_width;
                code_mask = (1 << code_width) - 1;
            }
        }
        have_old_code = 1;
        if (clear_each != 0 && pixel_index + 1u < pixel_count) {
            edge_lzw_code(compressed,
                          sizeof(compressed),
                          &compressed_length,
                          &bits,
                          &valid_bits,
                          4u,
                          code_width,
                          &failed);
            code_width = 3;
            code_mask = 7;
            available = 6;
            have_old_code = 0;
        }
    }
    edge_lzw_code(compressed,
                  sizeof(compressed),
                  &compressed_length,
                  &bits,
                  &valid_bits,
                  5u,
                  code_width,
                  &failed);
    if (valid_bits > 0) {
        if (compressed_length >= sizeof(compressed)) {
            failed = 1;
        } else {
            compressed[compressed_length++] = (unsigned char)bits;
        }
    }
    if (failed != 0) {
        writer->failed = 1;
        return;
    }

    while (block_offset < compressed_length) {
        block_size = compressed_length - block_offset;
        if (block_size > 255u) {
            block_size = 255u;
        }
        edge_put_u8(writer, (unsigned int)block_size);
        edge_put_bytes(writer,
                       compressed + block_offset,
                       block_size);
        block_offset += block_size;
    }
    edge_put_u8(writer, 0u);
}

void
edge_gif_image(edge_writer_t *writer,
               unsigned int x,
               unsigned int y,
               unsigned int width,
               unsigned int height,
               int interlaced,
               unsigned char const *pixels,
               size_t pixel_count,
               int clear_each)
{
    edge_gif_image_core(writer,
                        x,
                        y,
                        width,
                        height,
                        interlaced,
                        NULL,
                        pixels,
                        pixel_count,
                        clear_each);
}

void
edge_gif_local_image(edge_writer_t *writer,
                     unsigned int x,
                     unsigned int y,
                     unsigned int width,
                     unsigned int height,
                     unsigned char const *palette,
                     unsigned char const *pixels,
                     size_t pixel_count)
{
    edge_gif_image_core(writer,
                        x,
                        y,
                        width,
                        height,
                        0,
                        palette,
                        pixels,
                        pixel_count,
                        1);
}

void
edge_gif_graphic_control(edge_writer_t *writer, unsigned int disposal)
{
    edge_put_u8(writer, 0x21u);
    edge_put_u8(writer, 0xf9u);
    edge_put_u8(writer, 4u);
    edge_put_u8(writer, (disposal & 7u) << 2);
    edge_put_u16le(writer, 0u);
    edge_put_u8(writer, 0u);
    edge_put_u8(writer, 0u);
}
