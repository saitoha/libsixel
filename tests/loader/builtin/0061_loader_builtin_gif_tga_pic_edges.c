/*
 * Exercise builtin GIF, TGA, and PIC decoder branches with tiny in-memory
 * streams. Each TAP wrapper selects exactly one case, so failures retain a
 * narrow purpose without paying for fixture creation or helper processes.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "tests/loader/pixelformat_test_common.h"
#include "src/cms.h"
#include "src/factory.h"
#include "src/loader.h"
#include "src/loader-common.h"

#define EDGE_BUFFER_CAPACITY 65536u
#define EDGE_FRAME_CAPACITY 2
#define EDGE_RGB_CAPACITY 12288u

typedef struct edge_writer {
    unsigned char *buffer;
    size_t capacity;
    size_t length;
    int failed;
} edge_writer_t;

typedef struct edge_frame_probe {
    int callback_count;
    int width[EDGE_FRAME_CAPACITY];
    int height[EDGE_FRAME_CAPACITY];
    int pixelformat[EDGE_FRAME_CAPACITY];
    size_t rgb_size[EDGE_FRAME_CAPACITY];
    unsigned char rgb[EDGE_FRAME_CAPACITY][EDGE_RGB_CAPACITY];
} edge_frame_probe_t;

typedef int (*edge_case_fn_t)(void);

typedef struct edge_case_entry {
    char const *name;
    edge_case_fn_t fn;
} edge_case_entry_t;

static unsigned char const edge_palette_rgb[12] = {
    0x00u, 0x00u, 0x00u,
    0xffu, 0x00u, 0x00u,
    0x00u, 0xffu, 0x00u,
    0x00u, 0x00u, 0xffu
};

static void
edge_put_u8(edge_writer_t *writer, unsigned int value)
{
    if (writer == NULL || writer->failed != 0) {
        return;
    }
    if (writer->length >= writer->capacity) {
        writer->failed = 1;
        return;
    }
    writer->buffer[writer->length++] = (unsigned char)value;
}

static void
edge_put_u16le(edge_writer_t *writer, unsigned int value)
{
    edge_put_u8(writer, value & 0xffu);
    edge_put_u8(writer, (value >> 8) & 0xffu);
}

static void
edge_put_u16be(edge_writer_t *writer, unsigned int value)
{
    edge_put_u8(writer, (value >> 8) & 0xffu);
    edge_put_u8(writer, value & 0xffu);
}

static void
edge_put_bytes(edge_writer_t *writer,
               unsigned char const *bytes,
               size_t byte_count)
{
    if (writer == NULL || bytes == NULL || writer->failed != 0) {
        return;
    }
    if (writer->length > writer->capacity ||
        byte_count > writer->capacity - writer->length) {
        writer->failed = 1;
        return;
    }
    memcpy(writer->buffer + writer->length, bytes, byte_count);
    writer->length += byte_count;
}

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

static void
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
    edge_put_u8(writer, interlaced != 0 ? 0x40u : 0u);
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

static void
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

static void
edge_tga_begin(edge_writer_t *writer,
               unsigned int color_map_type,
               unsigned int image_type,
               unsigned int palette_length,
               unsigned int palette_depth,
               unsigned int width,
               unsigned int height,
               unsigned int pixel_depth,
               unsigned int descriptor)
{
    edge_put_u8(writer, 0u);
    edge_put_u8(writer, color_map_type);
    edge_put_u8(writer, image_type);
    edge_put_u16le(writer, 0u);
    edge_put_u16le(writer, palette_length);
    edge_put_u8(writer, palette_depth);
    edge_put_u16le(writer, 0u);
    edge_put_u16le(writer, 0u);
    edge_put_u16le(writer, width);
    edge_put_u16le(writer, height);
    edge_put_u8(writer, pixel_depth);
    edge_put_u8(writer, descriptor);
}

static void
edge_pic_begin(edge_writer_t *writer,
               unsigned int width,
               unsigned int height)
{
    static unsigned char const magic[4] = {
        0x53u, 0x80u, 0xf6u, 0x34u
    };
    static unsigned char const pict[4] = { 'P', 'I', 'C', 'T' };
    unsigned int index;

    index = 0u;
    edge_put_bytes(writer, magic, sizeof(magic));
    for (index = 0u; index < 84u; ++index) {
        edge_put_u8(writer, 0u);
    }
    edge_put_bytes(writer, pict, sizeof(pict));
    edge_put_u16be(writer, width);
    edge_put_u16be(writer, height);
    for (index = 0u; index < 8u; ++index) {
        edge_put_u8(writer, 0u);
    }
}

static void
edge_pic_packet(edge_writer_t *writer,
                int chained,
                unsigned int type,
                unsigned int channels)
{
    edge_put_u8(writer, chained != 0 ? 1u : 0u);
    edge_put_u8(writer, 8u);
    edge_put_u8(writer, type);
    edge_put_u8(writer, channels);
}

static SIXELSTATUS
edge_capture_frame(sixel_frame_t *frame, void *data)
{
    edge_frame_probe_t *probe;
    unsigned char const *pixels;
    size_t rgb_size;
    int frame_index;
    int width;
    int height;
    int pixelformat;

    probe = (edge_frame_probe_t *)data;
    pixels = NULL;
    rgb_size = 0u;
    frame_index = 0;
    width = 0;
    height = 0;
    pixelformat = 0;
    if (frame == NULL || probe == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    frame_index = probe->callback_count;
    if (frame_index >= EDGE_FRAME_CAPACITY) {
        return SIXEL_BAD_INPUT;
    }

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    pixelformat = sixel_frame_get_pixelformat(frame);
    probe->width[frame_index] = width;
    probe->height[frame_index] = height;
    probe->pixelformat[frame_index] = pixelformat;
    ++probe->callback_count;
    if (width <= 0 || height <= 0 ||
        pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
        (size_t)width > SIZE_MAX / (size_t)height ||
        (size_t)width * (size_t)height > EDGE_RGB_CAPACITY / 3u) {
        return SIXEL_BAD_INPUT;
    }
    rgb_size = (size_t)width * (size_t)height * 3u;
    pixels = sixel_frame_get_pixels(frame);
    if (pixels == NULL) {
        return SIXEL_BAD_INPUT;
    }
    memcpy(probe->rgb[frame_index], pixels, rgb_size);
    probe->rgb_size[frame_index] = rgb_size;
    return SIXEL_OK;
}

static int
edge_load_buffer(char const *label,
                 unsigned char const *buffer,
                 size_t buffer_size,
                 int require_static,
                 edge_frame_probe_t *probe,
                 SIXELSTATUS *load_status)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_loader_component_t *component;
    sixel_chunk_t *chunk;
    loader_probe_callback_state_t callback_state;
    int use_palette;
    int reqcolors;
    int loop_control;
    int cms_engine;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    component = NULL;
    chunk = NULL;
    use_palette = 0;
    reqcolors = 256;
    loop_control = SIXEL_LOOP_DISABLE;
    cms_engine = SIXEL_CMS_ENGINE_NONE;
    result = 1;
    if (load_status != NULL) {
        *load_status = SIXEL_FALSE;
    }
    if (label == NULL || buffer == NULL || buffer_size == 0u ||
        probe == NULL || load_status == NULL) {
        return 1;
    }
    memset(probe, 0, sizeof(*probe));

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: allocator initialization failed\n", label);
        return 1;
    }
    status = create_loader_component_by_name("builtin",
                                             allocator,
                                             (void **)&component);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: builtin component creation failed\n", label);
        goto cleanup;
    }
    status = sixel_chunk_create_from_memory(&chunk,
                                            buffer,
                                            buffer_size,
                                            NULL,
                                            allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: memory chunk creation failed\n", label);
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                           &require_static);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_USE_PALETTE,
                                           &use_palette);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQCOLORS,
                                           &reqcolors);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                           &loop_control);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(
        component,
        SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE,
        &cms_engine);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    callback_state.loader = NULL;
    callback_state.fn = edge_capture_frame;
    callback_state.context = probe;
    *load_status = sixel_loader_component_load(component,
                                               chunk,
                                               capture_frame_trampoline,
                                               &callback_state);
    result = 0;

cleanup:
    sixel_loader_component_unref(component);
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_allocator_unref(allocator);
    return result;
}

static int
edge_expect_rgb(char const *label,
                unsigned char const *buffer,
                size_t buffer_size,
                int require_static,
                int expected_width,
                int expected_height,
                int expected_frames,
                unsigned char const *expected_rgb,
                size_t expected_frame_size)
{
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    int frame_index;
    int result;

    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    frame_index = 0;
    result = edge_load_buffer(label,
                              buffer,
                              buffer_size,
                              require_static,
                              &probe,
                              &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: loader failed (%d)\n", label, (int)status);
        return 1;
    }
    if (probe.callback_count != expected_frames) {
        fprintf(stderr,
                "%s: callback count mismatch (%d, expected %d)\n",
                label,
                probe.callback_count,
                expected_frames);
        return 1;
    }
    for (frame_index = 0;
         frame_index < expected_frames;
         ++frame_index) {
        if (probe.width[frame_index] != expected_width ||
            probe.height[frame_index] != expected_height ||
            probe.pixelformat[frame_index] != SIXEL_PIXELFORMAT_RGB888 ||
            probe.rgb_size[frame_index] != expected_frame_size) {
            fprintf(stderr, "%s: frame %d metadata mismatch\n",
                    label, frame_index);
            return 1;
        }
        if (expected_rgb != NULL &&
            memcmp(probe.rgb[frame_index],
                   expected_rgb + (size_t)frame_index * expected_frame_size,
                   expected_frame_size) != 0) {
            fprintf(stderr, "%s: frame %d RGB mismatch\n",
                    label, frame_index);
            return 1;
        }
    }
    return 0;
}

static int
edge_expect_failure(char const *label,
                    unsigned char const *buffer,
                    size_t buffer_size)
{
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    int result;

    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    result = edge_load_buffer(label,
                              buffer,
                              buffer_size,
                              1,
                              &probe,
                              &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr, "%s: malformed stream unexpectedly succeeded\n",
                label);
        return 1;
    }
    if (probe.callback_count != 0) {
        fprintf(stderr, "%s: malformed stream emitted a frame\n", label);
        return 1;
    }
    return 0;
}

static int
edge_gif87(void)
{
    unsigned char buffer[128];
    unsigned char const pixels[2] = { 1u, 2u };
    unsigned char const expected[6] = {
        0xffu, 0x00u, 0x00u,
        0x00u, 0xffu, 0x00u
    };
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_gif_begin(&writer, 0, 2u, 1u);
    edge_gif_image(&writer, 0u, 0u, 2u, 1u, 0, pixels, 2u, 1);
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("GIF87a decode",
                           buffer,
                           writer.length,
                           1,
                           2,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}

static int
edge_gif_interlace(void)
{
    unsigned char buffer[160];
    unsigned char const encoded_rows[4] = { 1u, 3u, 2u, 0u };
    unsigned char const expected[12] = {
        0xffu, 0x00u, 0x00u,
        0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu,
        0x00u, 0x00u, 0x00u
    };
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_gif_begin(&writer, 1, 1u, 4u);
    edge_gif_image(&writer,
                   0u,
                   0u,
                   1u,
                   4u,
                   1,
                   encoded_rows,
                   4u,
                   1);
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("GIF interlace four-pass row order",
                           buffer,
                           writer.length,
                           1,
                           1,
                           4,
                           1,
                           expected,
                           sizeof(expected));
}

static int
edge_gif_offset(void)
{
    unsigned char buffer[160];
    unsigned char const pixels[1] = { 1u };
    unsigned char const expected[18] = {
        0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u,
        0xffu, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u
    };
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_gif_begin(&writer, 1, 3u, 2u);
    edge_gif_image(&writer, 1u, 1u, 1u, 1u, 0, pixels, 1u, 1);
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("GIF image rectangle offset",
                           buffer,
                           writer.length,
                           1,
                           3,
                           2,
                           1,
                           expected,
                           sizeof(expected));
}

static int
edge_gif_offset_oob(void)
{
    unsigned char buffer[160];
    unsigned char const pixels[2] = { 1u, 1u };
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_gif_begin(&writer, 1, 2u, 1u);
    edge_gif_image(&writer, 1u, 0u, 2u, 1u, 0, pixels, 2u, 1);
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_failure("GIF out-of-bounds rectangle",
                               buffer,
                               writer.length);
}

static int
edge_gif_extensions(void)
{
    static unsigned char const comment[] = {
        0x21u, 0xfeu, 3u, 'a', 'b', 'c', 2u, 'd', 'e', 0u
    };
    static unsigned char const plain_text[] = {
        0x21u, 0x01u, 12u,
        0u, 0u, 0u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u,
        1u, 'x', 0u
    };
    static unsigned char const application[] = {
        0x21u, 0xffu, 11u,
        'E', 'D', 'G', 'E', 'T', 'E', 'S', 'T', '0', '0', '1',
        2u, 0xaau, 0x55u, 0u
    };
    unsigned char buffer[256];
    unsigned char const pixels[1] = { 2u };
    unsigned char const expected[3] = { 0u, 0xffu, 0u };
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_gif_begin(&writer, 1, 1u, 1u);
    edge_put_bytes(&writer, comment, sizeof(comment));
    edge_put_bytes(&writer, plain_text, sizeof(plain_text));
    edge_put_bytes(&writer, application, sizeof(application));
    edge_gif_image(&writer, 0u, 0u, 1u, 1u, 0, pixels, 1u, 1);
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("GIF extension sub-block skipping",
                           buffer,
                           writer.length,
                           1,
                           1,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}

static int
edge_gif_truncated_raster(void)
{
    unsigned char buffer[128];
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_gif_begin(&writer, 1, 1u, 1u);
    edge_put_u8(&writer, 0x2cu);
    edge_put_u16le(&writer, 0u);
    edge_put_u16le(&writer, 0u);
    edge_put_u16le(&writer, 1u);
    edge_put_u16le(&writer, 1u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 2u);
    edge_put_u8(&writer, 2u);
    edge_put_u8(&writer, 0x44u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_failure("GIF truncated raster sub-block",
                               buffer,
                               writer.length);
}

static int
edge_gif_illegal_code(void)
{
    unsigned char buffer[128];
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_gif_begin(&writer, 1, 1u, 1u);
    edge_put_u8(&writer, 0x2cu);
    edge_put_u16le(&writer, 0u);
    edge_put_u16le(&writer, 0u);
    edge_put_u16le(&writer, 1u);
    edge_put_u16le(&writer, 1u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 2u);
    edge_put_u8(&writer, 2u);
    edge_put_u8(&writer, 0x7cu);
    edge_put_u8(&writer, 0x01u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_failure("GIF illegal LZW dictionary code",
                               buffer,
                               writer.length);
}

static int
edge_gif_disposal2(void)
{
    unsigned char buffer[256];
    unsigned char const red[1] = { 1u };
    unsigned char const green[1] = { 2u };
    unsigned char const expected[12] = {
        0xffu, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u,
        0x00u, 0xffu, 0x00u
    };
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_gif_begin(&writer, 1, 2u, 1u);
    edge_gif_graphic_control(&writer, 2u);
    edge_gif_image(&writer, 0u, 0u, 1u, 1u, 0, red, 1u, 1);
    edge_gif_image(&writer, 1u, 0u, 1u, 1u, 0, green, 1u, 1);
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("GIF disposal method 2",
                           buffer,
                           writer.length,
                           0,
                           2,
                           1,
                           2,
                           expected,
                           6u);
}

static int
edge_gif_lzw12(void)
{
    unsigned char buffer[EDGE_BUFFER_CAPACITY];
    unsigned char pixels[4090];
    edge_writer_t writer;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    size_t index;
    size_t rgb_offset;
    unsigned int palette_index;
    int result;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    index = 0u;
    rgb_offset = 0u;
    palette_index = 0u;
    result = 1;
    for (index = 0u; index < sizeof(pixels); ++index) {
        pixels[index] = (unsigned char)(index & 3u);
    }
    edge_gif_begin(&writer, 1, 4090u, 1u);
    edge_gif_image(&writer,
                   0u,
                   0u,
                   4090u,
                   1u,
                   0,
                   pixels,
                   sizeof(pixels),
                   0);
    edge_put_u8(&writer, 0x3bu);
    if (writer.failed != 0) {
        return 1;
    }
    result = edge_load_buffer("GIF LZW 12-bit code width",
                              buffer,
                              writer.length,
                              1,
                              &probe,
                              &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status) || probe.callback_count != 1 ||
        probe.width[0] != 4090 || probe.height[0] != 1 ||
        probe.rgb_size[0] != 4090u * 3u) {
        fprintf(stderr, "GIF LZW 12-bit code width: metadata mismatch\n");
        return 1;
    }
    for (index = 0u; index < sizeof(pixels); ++index) {
        palette_index = (unsigned int)pixels[index];
        rgb_offset = index * 3u;
        if (memcmp(probe.rgb[0] + rgb_offset,
                   edge_palette_rgb + palette_index * 3u,
                   3u) != 0) {
            fprintf(stderr,
                    "GIF LZW 12-bit code width: pixel %zu mismatch\n",
                    index);
            return 1;
        }
    }
    return 0;
}

static int
edge_tga_rgb16(void)
{
    unsigned char buffer[64];
    unsigned char const expected[6] = {
        0xffu, 0x00u, 0x00u,
        0x00u, 0xffu, 0x00u
    };
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_tga_begin(&writer, 0u, 2u, 0u, 0u, 2u, 1u, 16u, 0x20u);
    edge_put_u16le(&writer, 0x7c00u);
    edge_put_u16le(&writer, 0x03e0u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("TGA 16-bit truecolor",
                           buffer,
                           writer.length,
                           1,
                           2,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}

static int
edge_tga_palette8(void)
{
    unsigned char buffer[64];
    unsigned char const expected[3] = { 0x7fu, 0x7fu, 0x7fu };
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_tga_begin(&writer, 1u, 1u, 1u, 8u, 1u, 1u, 8u, 0x20u);
    edge_put_u8(&writer, 0x7fu);
    edge_put_u8(&writer, 0u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("TGA 8-bit grayscale palette entry",
                           buffer,
                           writer.length,
                           1,
                           1,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}

static int
edge_tga_palette16(void)
{
    unsigned char buffer[64];
    unsigned char const expected[3] = { 0u, 0u, 0xffu };
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_tga_begin(&writer, 1u, 1u, 1u, 16u, 1u, 1u, 8u, 0x20u);
    edge_put_u16le(&writer, 0x001fu);
    edge_put_u8(&writer, 0u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("TGA 16-bit palette entry",
                           buffer,
                           writer.length,
                           1,
                           1,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}

static int
edge_tga_index16(void)
{
    unsigned char buffer[64];
    unsigned char const palette[6] = {
        0u, 0u, 0u,
        0u, 0u, 0xffu
    };
    unsigned char const expected[3] = { 0xffu, 0u, 0u };
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_tga_begin(&writer, 1u, 1u, 2u, 24u, 1u, 1u, 16u, 0x20u);
    edge_put_bytes(&writer, palette, sizeof(palette));
    edge_put_u16le(&writer, 1u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("TGA 16-bit palette index",
                           buffer,
                           writer.length,
                           1,
                           1,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}

static int
edge_tga_bottom_origin(void)
{
    unsigned char buffer[64];
    unsigned char const file_pixels[6] = {
        0xffu, 0x00u, 0x00u,
        0x00u, 0x00u, 0xffu
    };
    unsigned char const expected[6] = {
        0xffu, 0x00u, 0x00u,
        0x00u, 0x00u, 0xffu
    };
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_tga_begin(&writer, 0u, 2u, 0u, 0u, 1u, 2u, 24u, 0u);
    edge_put_bytes(&writer, file_pixels, sizeof(file_pixels));
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("TGA bottom-origin row order",
                           buffer,
                           writer.length,
                           1,
                           1,
                           2,
                           1,
                           expected,
                           sizeof(expected));
}

static int
edge_tga_rle_packets(void)
{
    unsigned char buffer[64];
    unsigned char const expected[9] = {
        0xffu, 0x00u, 0x00u,
        0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu
    };
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_tga_begin(&writer, 0u, 10u, 0u, 0u, 3u, 1u, 24u, 0x20u);
    edge_put_u8(&writer, 1u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0xffu);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0xffu);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0x80u);
    edge_put_u8(&writer, 0xffu);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("TGA raw and repeated RLE packets",
                           buffer,
                           writer.length,
                           1,
                           3,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}

static int
edge_tga_truncated_palette(void)
{
    unsigned char buffer[64];
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_tga_begin(&writer, 1u, 1u, 2u, 24u, 1u, 1u, 8u, 0x20u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_failure("TGA truncated palette",
                               buffer,
                               writer.length);
}

static int
edge_tga_oob_index(void)
{
    unsigned char buffer[64];
    unsigned char const expected[3] = { 0u, 0u, 0u };
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_tga_begin(&writer, 1u, 1u, 1u, 24u, 1u, 1u, 8u, 0x20u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 0u);
    edge_put_u8(&writer, 1u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("TGA out-of-range index compatibility fallback",
                           buffer,
                           writer.length,
                           1,
                           1,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}

static int
edge_pic_missing_channels(void)
{
    unsigned char buffer[160];
    unsigned char const expected[6] = {
        0x10u, 0xffu, 0xffu,
        0x20u, 0xffu, 0xffu
    };
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_pic_begin(&writer, 2u, 1u);
    edge_pic_packet(&writer, 0, 0u, 0x80u);
    edge_put_u8(&writer, 0x10u);
    edge_put_u8(&writer, 0x20u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("PIC omitted channels default to white",
                           buffer,
                           writer.length,
                           1,
                           2,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}

static int
edge_pic_overwrite(void)
{
    unsigned char buffer[160];
    unsigned char const expected[6] = {
        0x30u, 0xffu, 0xffu,
        0x40u, 0xffu, 0xffu
    };
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_pic_begin(&writer, 2u, 1u);
    edge_pic_packet(&writer, 1, 0u, 0x80u);
    edge_pic_packet(&writer, 0, 0u, 0x80u);
    edge_put_u8(&writer, 0x10u);
    edge_put_u8(&writer, 0x20u);
    edge_put_u8(&writer, 0x30u);
    edge_put_u8(&writer, 0x40u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("PIC repeated channel packet last write wins",
                           buffer,
                           writer.length,
                           1,
                           2,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}

static int
edge_pic_pure_clip(void)
{
    unsigned char buffer[160];
    unsigned char const expected[6] = {
        0x44u, 0xffu, 0xffu,
        0x44u, 0xffu, 0xffu
    };
    edge_writer_t writer;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    edge_pic_begin(&writer, 2u, 1u);
    edge_pic_packet(&writer, 0, 1u, 0x80u);
    edge_put_u8(&writer, 0xffu);
    edge_put_u8(&writer, 0x44u);
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("PIC pure RLE oversized run clipping",
                           buffer,
                           writer.length,
                           1,
                           2,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}

static int
edge_pic_mixed_raw128(void)
{
    unsigned char buffer[512];
    edge_writer_t writer;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    size_t index;
    int result;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    index = 0u;
    result = 1;
    edge_pic_begin(&writer, 128u, 1u);
    edge_pic_packet(&writer, 0, 2u, 0x80u);
    edge_put_u8(&writer, 127u);
    for (index = 0u; index < 128u; ++index) {
        edge_put_u8(&writer, (unsigned int)index);
    }
    if (writer.failed != 0) {
        return 1;
    }
    result = edge_load_buffer("PIC mixed RLE raw count 128",
                              buffer,
                              writer.length,
                              1,
                              &probe,
                              &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status) || probe.callback_count != 1 ||
        probe.width[0] != 128 || probe.height[0] != 1) {
        fprintf(stderr, "PIC mixed RLE raw count 128: metadata mismatch\n");
        return 1;
    }
    for (index = 0u; index < 128u; ++index) {
        if (probe.rgb[0][index * 3u] != (unsigned char)index ||
            probe.rgb[0][index * 3u + 1u] != 0xffu ||
            probe.rgb[0][index * 3u + 2u] != 0xffu) {
            fprintf(stderr,
                    "PIC mixed RLE raw count 128: pixel %zu mismatch\n",
                    index);
            return 1;
        }
    }
    return 0;
}

static int
edge_pic_mixed_repeat128(void)
{
    unsigned char buffer[160];
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    edge_writer_t writer;
    size_t index;
    int result;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    index = 0u;
    result = 1;
    edge_pic_begin(&writer, 128u, 1u);
    edge_pic_packet(&writer, 0, 2u, 0x80u);
    edge_put_u8(&writer, 255u);
    edge_put_u8(&writer, 0x5au);
    if (writer.failed != 0) {
        return 1;
    }
    result = edge_load_buffer("PIC mixed RLE repeated count 128",
                              buffer,
                              writer.length,
                              1,
                              &probe,
                              &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status) || probe.callback_count != 1 ||
        probe.width[0] != 128 || probe.height[0] != 1) {
        fprintf(stderr,
                "PIC mixed RLE repeated count 128: metadata mismatch\n");
        return 1;
    }
    for (index = 0u; index < 128u; ++index) {
        if (probe.rgb[0][index * 3u] != 0x5au ||
            probe.rgb[0][index * 3u + 1u] != 0xffu ||
            probe.rgb[0][index * 3u + 2u] != 0xffu) {
            fprintf(stderr,
                    "PIC mixed RLE repeated count 128: pixel %zu mismatch\n",
                    index);
            return 1;
        }
    }
    return 0;
}

static int
edge_pic_ten_packets(void)
{
    unsigned char buffer[192];
    unsigned char const expected[3] = { 9u, 0xffu, 0xffu };
    edge_writer_t writer;
    unsigned int packet_index;

    writer.buffer = buffer;
    writer.capacity = sizeof(buffer);
    writer.length = 0u;
    writer.failed = 0;
    packet_index = 0u;
    edge_pic_begin(&writer, 1u, 1u);
    for (packet_index = 0u; packet_index < 10u; ++packet_index) {
        edge_pic_packet(&writer,
                        packet_index + 1u < 10u,
                        0u,
                        0x80u);
    }
    for (packet_index = 0u; packet_index < 10u; ++packet_index) {
        edge_put_u8(&writer, packet_index);
    }
    if (writer.failed != 0) {
        return 1;
    }
    return edge_expect_rgb("PIC ten-packet upper bound",
                           buffer,
                           writer.length,
                           1,
                           1,
                           1,
                           1,
                           expected,
                           sizeof(expected));
}

int
test_loader_0061_loader_builtin_gif_tga_pic_edges(int argc, char **argv)
{
    static edge_case_entry_t const cases[] = {
        { "gif87", edge_gif87 },
        { "gif-interlace", edge_gif_interlace },
        { "gif-offset", edge_gif_offset },
        { "gif-offset-oob", edge_gif_offset_oob },
        { "gif-extensions", edge_gif_extensions },
        { "gif-truncated-raster", edge_gif_truncated_raster },
        { "gif-illegal-code", edge_gif_illegal_code },
        { "gif-disposal2", edge_gif_disposal2 },
        { "gif-lzw12", edge_gif_lzw12 },
        { "tga-rgb16", edge_tga_rgb16 },
        { "tga-palette8", edge_tga_palette8 },
        { "tga-palette16", edge_tga_palette16 },
        { "tga-index16", edge_tga_index16 },
        { "tga-bottom-origin", edge_tga_bottom_origin },
        { "tga-rle-packets", edge_tga_rle_packets },
        { "tga-truncated-palette", edge_tga_truncated_palette },
        { "tga-oob-index", edge_tga_oob_index },
        { "pic-missing-channels", edge_pic_missing_channels },
        { "pic-overwrite", edge_pic_overwrite },
        { "pic-pure-clip", edge_pic_pure_clip },
        { "pic-mixed-raw128", edge_pic_mixed_raw128 },
        { "pic-mixed-repeat128", edge_pic_mixed_repeat128 },
        { "pic-ten-packets", edge_pic_ten_packets }
    };
    char const *selected_case;
    size_t case_index;

    (void)argc;
    (void)argv;
    selected_case = sixel_compat_getenv("SIXEL_TEST_BUILTIN_EDGE_CASE");
    case_index = 0u;
    if (selected_case == NULL || selected_case[0] == '\0') {
        fprintf(stderr, "builtin edge test: case selector is missing\n");
        return 1;
    }
    for (case_index = 0u;
         case_index < sizeof(cases) / sizeof(cases[0]);
         ++case_index) {
        if (strcmp(selected_case, cases[case_index].name) == 0) {
            return cases[case_index].fn();
        }
    }
    fprintf(stderr, "builtin edge test: unknown case '%s'\n", selected_case);
    return 1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
