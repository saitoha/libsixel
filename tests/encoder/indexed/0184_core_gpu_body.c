/* SPDX-License-Identifier: MIT */
/* GPU band composition writes the same bytes as the CPU compose, and both
 * decode to the source registers, with a reserved key color, kept pixels
 * and offset bands. Without a GPU device both runs use the CPU compose. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/encoder-core.h"
#include "src/encode-result.h"
#include "src/palette.h"
#include "src/threading.h"

#define GPU_BODY_COLORS 40
#define GPU_BODY_KEY 5

typedef struct gpu_body_sink {
    unsigned char *bytes;
    size_t used;
    size_t size;
} gpu_body_sink_t;

static SIXELSTATUS
gpu_body_write(char const *bytes, size_t size, void *context)
{
    gpu_body_sink_t *sink;

    sink = context;
    if (size > sink->size - sink->used) {
        return SIXEL_RUNTIME_ERROR;
    }
    memcpy(sink->bytes + sink->used, bytes, size);
    sink->used += size;
    return SIXEL_OK;
}

static sixel_indexed_frame_t *
gpu_body_image(sixel_allocator_t *allocator)
{
    sixel_wire_color_definition_t colors[GPU_BODY_COLORS];
    sixel_wire_palette_request_t wire;
    sixel_indexed_frame_description_t desc;
    sixel_wire_palette_t *palette;
    sixel_indexed_frame_t *image;
    unsigned int seed;
    int x;
    int y;
    int i;

    for (i = 0; i < GPU_BODY_COLORS; ++i) {
        colors[i].register_index = (unsigned int)i;
        colors[i].type = 2;
        colors[i].fields[0] = (i * 37) % 101;
        colors[i].fields[1] = (i * 53) % 101;
        colors[i].fields[2] = (i * 71) % 101;
    }
    memset(&wire, 0, sizeof(wire));
    wire.definitions = colors;
    wire.count = GPU_BODY_COLORS;
    wire.color_definition = SIXEL_PALETTE_OUTPUT_LEGACY;
    wire.colorspace = SIXEL_COLORSPACE_GAMMA;
    palette = NULL;
    if (sixel_wire_palette_new(&palette, &wire, allocator) != SIXEL_OK) {
        return NULL;
    }
    /* The source starts at column 3 and row 5, so bands straddle it. */
    memset(&desc, 0, sizeof(desc));
    desc.width = 97;
    desc.height = 40;
    desc.encoded_x = 3;
    desc.encoded_y = 5;
    desc.encoded_width = desc.plane_width = 100;
    desc.encoded_height = desc.plane_height = 45;
    desc.palette = palette;
    desc.keycolor = GPU_BODY_KEY;
    desc.keycolor_reserved = 1;
    desc.transparent_background = 1;
    image = NULL;
    if (sixel_indexed_frame_new(&image, &desc, allocator) == SIXEL_OK) {
        seed = 12345U;
        for (y = 0; y < desc.height; ++y) {
            for (x = 0; x < desc.width; ++x) {
                i = y * desc.width + x;
                seed = seed * 1103515245U + 12345U;
                /* Mix long runs, short gaps and scattered registers. */
                image->indexes[i] = (sixel_index_t)((x / 13 + y / 4) % 7 == 0
                    ? (seed >> 16) % GPU_BODY_COLORS
                    : (x / 9 + y) % GPU_BODY_COLORS);
                image->skip[i] = (seed >> 8) % 11 == 0
                    ? SIXEL_INDEXED_KEEP_APPROXIMATE : SIXEL_INDEXED_PAINT;
                /* Painted pixels never carry the key color; kept ones
                 * may, and must stay unpainted. */
                if (image->indexes[i] == GPU_BODY_KEY &&
                        image->skip[i] == SIXEL_INDEXED_PAINT) {
                    image->indexes[i] = GPU_BODY_KEY + 1;
                } else if ((seed >> 4) % 5 == 0 &&
                        image->skip[i] != SIXEL_INDEXED_PAINT) {
                    image->indexes[i] = GPU_BODY_KEY;
                }
            }
        }
    }
    sixel_wire_palette_unref(palette);
    return image;
}

static SIXELSTATUS
gpu_body_encode(sixel_allocator_t *allocator, sixel_indexed_frame_t *image,
    int gpu, gpu_body_sink_t *sink)
{
    sixel_encoder_core_prepare_request_t request;
    sixel_encoder_core_t *core;
    void *object;
    SIXELSTATUS status;

    object = NULL;
    status = sixel_encoder_core_factory_new(allocator, &object);
    if (status != SIXEL_OK) return status;
    core = object;
    sixel_encoder_core_set_gpu_body(core, gpu);
    memset(&request, 0, sizeof(request));
    request.image = image;
    request.threads = 4;
    request.worker_capacity = 4;
    request.queue_bands = 8;
    request.options.emit_palette = 1;
    request.options.encode_policy = SIXEL_ENCODEPOLICY_FAST;
    request.sink.write = gpu_body_write;
    request.sink.context = sink;
    status = core->vtbl->prepare(core, &request);
    if (status == SIXEL_OK) {
        status = core->vtbl->consume(core, image->description.height);
    }
    if (status == SIXEL_OK) {
        status = core->vtbl->finish(core);
    } else {
        (void)core->vtbl->abort(core);
    }
    core->vtbl->unref(core);
    return status;
}

static unsigned char *
gpu_body_decode(gpu_body_sink_t const *sink, int *width, int *height)
{
    unsigned char *sixel;
    unsigned char *pixels;
    unsigned char *palette;
    char head[64];
    int head_size;
    int ncolors;
    size_t size;

    head_size = snprintf(head, sizeof(head), "\033P0;1q\"1;1;100;45");
    size = (size_t)head_size + sink->used + 2U;
    sixel = malloc(size);
    if (sixel == NULL) return NULL;
    memcpy(sixel, head, (size_t)head_size);
    memcpy(sixel + head_size, sink->bytes, sink->used);
    memcpy(sixel + head_size + sink->used, "\033\\", 2);
    pixels = NULL;
    palette = NULL;
    if (sixel_decode_raw(sixel, (int)size, &pixels, width, height,
            &palette, &ncolors, NULL) != SIXEL_OK) {
        pixels = NULL;
    }
    free(sixel);
    free(palette);
    return pixels;
}

int
test_0184_core_gpu_body(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS
    sixel_allocator_t *allocator;
    sixel_indexed_frame_t *image;
    gpu_body_sink_t cpu;
    gpu_body_sink_t gpu;
    unsigned char *cpu_pixels;
    unsigned char *gpu_pixels;
    int cpu_width, cpu_height, gpu_width, gpu_height;
    int x, y, i, painted;
    int ok;

    (void)argc;
    (void)argv;
    allocator = NULL;
    image = NULL;
    cpu_pixels = gpu_pixels = NULL;
    memset(&cpu, 0, sizeof(cpu));
    memset(&gpu, 0, sizeof(gpu));
    ok = 0;
    cpu.size = gpu.size = 1U << 20;
    cpu.bytes = malloc(cpu.size);
    gpu.bytes = malloc(gpu.size);
    if (cpu.bytes == NULL || gpu.bytes == NULL) goto end;
    if (sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL)
            != SIXEL_OK) goto end;
    image = gpu_body_image(allocator);
    if (image == NULL ||
            sixel_indexed_frame_publish(image, image->description.height)
                != SIXEL_OK ||
            sixel_indexed_frame_complete(image) != SIXEL_OK) {
        fprintf(stderr, "image setup failed\n");
        goto end;
    }
    if (gpu_body_encode(allocator, image, 0, &cpu) != SIXEL_OK ||
            gpu_body_encode(allocator, image, 1, &gpu) != SIXEL_OK) {
        fprintf(stderr, "encode failed\n");
        goto end;
    }
    if (cpu.used != gpu.used || memcmp(cpu.bytes, gpu.bytes, cpu.used) != 0) {
        fprintf(stderr, "GPU body bytes differ from the CPU compose\n");
        goto end;
    }
    cpu_pixels = gpu_body_decode(&cpu, &cpu_width, &cpu_height);
    gpu_pixels = gpu_body_decode(&gpu, &gpu_width, &gpu_height);
    if (cpu_pixels == NULL || gpu_pixels == NULL ||
            cpu_width != 100 || cpu_height != 45 ||
            gpu_width != cpu_width || gpu_height != cpu_height) {
        fprintf(stderr, "decode failed\n");
        goto end;
    }
    if (memcmp(cpu_pixels, gpu_pixels, (size_t)cpu_width * cpu_height)
            != 0) {
        fprintf(stderr, "GPU body decodes to different pixels\n");
        goto end;
    }
    /* Both decodes must also carry every painted source register. */
    painted = 0;
    for (y = 0; y < image->description.height; ++y) {
        for (x = 0; x < image->description.width; ++x) {
            i = y * image->description.width + x;
            if (image->skip[i] != SIXEL_INDEXED_PAINT ||
                    image->indexes[i] == GPU_BODY_KEY) continue;
            if (gpu_pixels[(y + 5) * 100 + x + 3] != image->indexes[i]) {
                fprintf(stderr, "pixel %d,%d lost its register\n", x, y);
                goto end;
            }
            ++painted;
        }
    }
    ok = painted > 0;
end:
    free(cpu_pixels);
    free(gpu_pixels);
    free(cpu.bytes);
    free(gpu.bytes);
    if (image != NULL) sixel_indexed_frame_unref(image);
    if (allocator != NULL) sixel_allocator_unref(allocator);
    return ok ? 0 : 1;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
