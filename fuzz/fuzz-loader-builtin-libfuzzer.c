/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * libFuzzer target for the builtin loader component.
 *
 * This harness intentionally goes through the loader factory/component API:
 *   factory -> create("builtin") -> setopt(...) -> load(...)
 * so it exercises the same wiring used by the production loader pipeline.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "chunk.h"
#include "loader-common.h"
#include "loader-component.h"
#include "loader-factory.h"

static sixel_allocator_t *g_allocator = NULL;
static sixel_loader_factory_t *g_factory = NULL;
static sixel_loader_component_t *g_component = NULL;
static int g_fuzz_ready = 0;
static unsigned char g_empty_input[1];
static unsigned char *g_forced_chunk_buffer = NULL;
static size_t g_forced_chunk_capacity = 0u;

typedef enum fuzz_builtin_force_format {
    FUZZ_FORCE_FORMAT_AUTO = 0,
    FUZZ_FORCE_FORMAT_PNG,
    FUZZ_FORCE_FORMAT_GIF,
    FUZZ_FORCE_FORMAT_JPEG,
    FUZZ_FORCE_FORMAT_PNM,
    FUZZ_FORCE_FORMAT_SIXEL,
    FUZZ_FORCE_FORMAT_HDR,
    FUZZ_FORCE_FORMAT_PSD,
    FUZZ_FORCE_FORMAT_PIC,
    FUZZ_FORCE_FORMAT_BMP,
    FUZZ_FORCE_FORMAT_WEBP
} fuzz_builtin_force_format_t;

static fuzz_builtin_force_format_t g_force_format = FUZZ_FORCE_FORMAT_AUTO;

static fuzz_builtin_force_format_t
fuzz_parse_force_format(char const *value)
{
    if (value == NULL || value[0] == '\0') {
        return FUZZ_FORCE_FORMAT_AUTO;
    }
    if (strcmp(value, "auto") == 0 || strcmp(value, "any") == 0) {
        return FUZZ_FORCE_FORMAT_AUTO;
    }
    if (strcmp(value, "png") == 0) {
        return FUZZ_FORCE_FORMAT_PNG;
    }
    if (strcmp(value, "gif") == 0) {
        return FUZZ_FORCE_FORMAT_GIF;
    }
    if (strcmp(value, "jpeg") == 0 || strcmp(value, "jpg") == 0) {
        return FUZZ_FORCE_FORMAT_JPEG;
    }
    if (strcmp(value, "pnm") == 0) {
        return FUZZ_FORCE_FORMAT_PNM;
    }
    if (strcmp(value, "sixel") == 0) {
        return FUZZ_FORCE_FORMAT_SIXEL;
    }
    if (strcmp(value, "hdr") == 0) {
        return FUZZ_FORCE_FORMAT_HDR;
    }
    if (strcmp(value, "psd") == 0) {
        return FUZZ_FORCE_FORMAT_PSD;
    }
    if (strcmp(value, "pic") == 0) {
        return FUZZ_FORCE_FORMAT_PIC;
    }
    if (strcmp(value, "bmp") == 0) {
        return FUZZ_FORCE_FORMAT_BMP;
    }
    if (strcmp(value, "webp") == 0) {
        return FUZZ_FORCE_FORMAT_WEBP;
    }
    return FUZZ_FORCE_FORMAT_AUTO;
}

static int
fuzz_forced_chunk_reserve(size_t size)
{
    unsigned char *next;

    if (size == 0u) {
        size = 1u;
    }
    if (size <= g_forced_chunk_capacity) {
        return 1;
    }

    next = (unsigned char *)realloc(g_forced_chunk_buffer, size);
    if (next == NULL) {
        return 0;
    }
    g_forced_chunk_buffer = next;
    g_forced_chunk_capacity = size;
    return 1;
}

static void
fuzz_apply_magic(unsigned char *buffer,
                 size_t size,
                 size_t offset,
                 unsigned char const *magic,
                 size_t magic_size)
{
    size_t copy_size;

    if (buffer == NULL || magic == NULL || offset >= size || magic_size == 0u) {
        return;
    }
    copy_size = magic_size;
    if (copy_size > size - offset) {
        copy_size = size - offset;
    }
    if (copy_size > 0u) {
        memcpy(buffer + offset, magic, copy_size);
    }
}

static int
fuzz_prepare_input_forced_format(uint8_t const *data,
                                 size_t size,
                                 unsigned char const **out_data,
                                 size_t *out_size)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    static unsigned char const gif_header[13] = {
        'G', 'I', 'F', '8', '9', 'a',
        0x01u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const jpeg_header[4] = {
        0xffu, 0xd8u, 0xffu, 0xd9u
    };
    static unsigned char const pnm_prefix[11] = {
        'P', '6', '\n', '1', ' ', '1', '\n', '2', '5', '5', '\n'
    };
    static unsigned char const sixel_prefix[5] = {
        0x1bu, 0x50u, 0x71u, 0x1bu, 0x5cu
    };
    static unsigned char const hdr_template[] =
        "#?RADIANCE\n"
        "FORMAT=32-bit_rle_rgbe\n"
        "\n"
        "-Y 1 +X 1\n"
        "\x80\x80\x80\x80";
    static unsigned char const psd_header[40] = {
        '8', 'B', 'P', 'S', 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x08u, 0x00u, 0x03u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u
    };
    static unsigned char const pic_magic_start[4] = {
        0x53u, 0x80u, 0xf6u, 0x34u
    };
    static unsigned char const pic_magic_mark[4] = {
        'P', 'I', 'C', 'T'
    };
    static unsigned char const bmp_header[58] = {
        'B', 'M', 0x3au, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x36u, 0x00u, 0x00u, 0x00u, 0x28u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u,
        0x18u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x04u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u
    };
    static unsigned char const webp_vp8l_header[36] = {
        'R', 'I', 'F', 'F', 0x1cu, 0x00u, 0x00u, 0x00u,
        'W', 'E', 'B', 'P', 'V', 'P', '8', 'L',
        0x0fu, 0x00u, 0x00u, 0x00u,
        0x2fu, 0x0fu, 0xc0u, 0x03u, 0x00u, 0x07u, 0x10u, 0xfdu,
        0x8fu, 0xfeu, 0x07u, 0x22u, 0xa2u, 0xffu, 0x01u, 0x00u
    };
    static unsigned char const webp_vp8x_header[54] = {
        'R', 'I', 'F', 'F', 0x2eu, 0x00u, 0x00u, 0x00u,
        'W', 'E', 'B', 'P', 'V', 'P', '8', 'X',
        0x0au, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x0fu, 0x00u, 0x00u, 0x0fu,
        0x00u, 0x00u, 'V', 'P', '8', 'L', 0x0fu, 0x00u, 0x00u, 0x00u,
        0x2fu, 0x0fu, 0xc0u, 0x03u, 0x00u, 0x07u, 0x10u, 0xfdu,
        0x8fu, 0xfeu, 0x07u, 0x22u, 0xa2u, 0xffu, 0x01u, 0x00u
    };
    size_t target_size;

    if (out_data == NULL || out_size == NULL) {
        return 0;
    }

    if (g_force_format == FUZZ_FORCE_FORMAT_AUTO) {
        *out_data = (unsigned char const *)(uintptr_t)data;
        *out_size = size;
        return 1;
    }

    target_size = size;
    switch (g_force_format) {
    case FUZZ_FORCE_FORMAT_PNG:
        if (target_size < sizeof(png_signature)) {
            target_size = sizeof(png_signature);
        }
        break;
    case FUZZ_FORCE_FORMAT_GIF:
        if (target_size < sizeof(gif_header)) {
            target_size = sizeof(gif_header);
        }
        break;
    case FUZZ_FORCE_FORMAT_JPEG:
        if (target_size < sizeof(jpeg_header)) {
            target_size = sizeof(jpeg_header);
        }
        break;
    case FUZZ_FORCE_FORMAT_PNM:
        if (target_size < sizeof(pnm_prefix) + 3u) {
            target_size = sizeof(pnm_prefix) + 3u;
        }
        break;
    case FUZZ_FORCE_FORMAT_SIXEL:
        if (target_size < sizeof(sixel_prefix)) {
            target_size = sizeof(sixel_prefix);
        }
        break;
    case FUZZ_FORCE_FORMAT_HDR:
        if (target_size < sizeof(hdr_template) - 1u) {
            target_size = sizeof(hdr_template) - 1u;
        }
        break;
    case FUZZ_FORCE_FORMAT_PSD:
        if (target_size < sizeof(psd_header) + 3u) {
            target_size = sizeof(psd_header) + 3u;
        }
        break;
    case FUZZ_FORCE_FORMAT_PIC:
        if (target_size < 92u) {
            target_size = 92u;
        }
        break;
    case FUZZ_FORCE_FORMAT_BMP:
        if (target_size < sizeof(bmp_header)) {
            target_size = sizeof(bmp_header);
        }
        break;
    case FUZZ_FORCE_FORMAT_WEBP:
        if (target_size < sizeof(webp_vp8x_header)) {
            target_size = sizeof(webp_vp8x_header);
        }
        break;
    case FUZZ_FORCE_FORMAT_AUTO:
    default:
        break;
    }

    if (!fuzz_forced_chunk_reserve(target_size)) {
        return 0;
    }

    if (size > 0u && data != NULL) {
        memcpy(g_forced_chunk_buffer, data, size);
    }
    if (target_size > size) {
        memset(g_forced_chunk_buffer + size, 0, target_size - size);
    }

    switch (g_force_format) {
    case FUZZ_FORCE_FORMAT_PNG:
        fuzz_apply_magic(g_forced_chunk_buffer,
                         target_size,
                         0u,
                         png_signature,
                         sizeof(png_signature));
        break;
    case FUZZ_FORCE_FORMAT_GIF:
        fuzz_apply_magic(g_forced_chunk_buffer,
                         target_size,
                         0u,
                         gif_header,
                         sizeof(gif_header));
        break;
    case FUZZ_FORCE_FORMAT_JPEG:
        fuzz_apply_magic(g_forced_chunk_buffer,
                         target_size,
                         0u,
                         jpeg_header,
                         sizeof(jpeg_header));
        break;
    case FUZZ_FORCE_FORMAT_PNM:
        fuzz_apply_magic(g_forced_chunk_buffer,
                         target_size,
                         0u,
                         pnm_prefix,
                         sizeof(pnm_prefix));
        break;
    case FUZZ_FORCE_FORMAT_SIXEL:
        fuzz_apply_magic(g_forced_chunk_buffer,
                         target_size,
                         0u,
                         sixel_prefix,
                         sizeof(sixel_prefix));
        break;
    case FUZZ_FORCE_FORMAT_HDR:
        fuzz_apply_magic(g_forced_chunk_buffer,
                         target_size,
                         0u,
                         hdr_template,
                         sizeof(hdr_template) - 1u);
        break;
    case FUZZ_FORCE_FORMAT_PSD:
        fuzz_apply_magic(g_forced_chunk_buffer,
                         target_size,
                         0u,
                         psd_header,
                         sizeof(psd_header));
        break;
    case FUZZ_FORCE_FORMAT_PIC:
        fuzz_apply_magic(g_forced_chunk_buffer,
                         target_size,
                         0u,
                         pic_magic_start,
                         sizeof(pic_magic_start));
        fuzz_apply_magic(g_forced_chunk_buffer,
                         target_size,
                         88u,
                         pic_magic_mark,
                         sizeof(pic_magic_mark));
        break;
    case FUZZ_FORCE_FORMAT_BMP:
        fuzz_apply_magic(g_forced_chunk_buffer,
                         target_size,
                         0u,
                         bmp_header,
                         sizeof(bmp_header));
        break;
    case FUZZ_FORCE_FORMAT_WEBP:
        fuzz_apply_magic(g_forced_chunk_buffer,
                         target_size,
                         0u,
                         (size > 0u && (data[0] & 1u) != 0u)
                         ? webp_vp8l_header
                         : webp_vp8x_header,
                         (size > 0u && (data[0] & 1u) != 0u)
                         ? sizeof(webp_vp8l_header)
                         : sizeof(webp_vp8x_header));
        break;
    case FUZZ_FORCE_FORMAT_AUTO:
    default:
        break;
    }

    *out_data = g_forced_chunk_buffer;
    *out_size = target_size;
    return 1;
}

static uint64_t
fuzz_data_mix64(uint8_t const *data, size_t size)
{
    uint64_t h;
    size_t i;

    h = UINT64_C(1469598103934665603);
    if (data == NULL) {
        return h;
    }

    for (i = 0; i < size; ++i) {
        h ^= (uint64_t)data[i];
        h *= UINT64_C(1099511628211);
    }
    h ^= (uint64_t)size;

    return h;
}

static int
fuzz_pick_reqcolors(unsigned char byte)
{
    return 2 + ((int)byte % (SIXEL_PALETTE_MAX - 1));
}

static void
fuzz_pick_loader_options(uint8_t const *data, size_t size,
                         int *fstatic, int *fuse_palette, int *reqcolors,
                         int *loop_control, int *start_frame_no,
                         int *enable_cms, int *use_bgcolor,
                         unsigned char bgcolor[3])
{
    uint64_t h;

    /*
     * Keep the full input as decoder payload and derive runtime knobs from a
     * stable hash so magic bytes at the beginning are not consumed as options.
     */
    h = fuzz_data_mix64(data, size);
    *fstatic = (int)(h & UINT64_C(1));
    *fuse_palette = (int)((h >> 1) & UINT64_C(1));
    *reqcolors = fuzz_pick_reqcolors((unsigned char)(h >> 8));
    /*
     * Fuzzing animated formats should never re-enter frame loops.
     * Keep animation traversal finite and deterministic.
     */
    *loop_control = SIXEL_LOOP_DISABLE;
    /*
     * Keep frame selection bounded while still covering negative offsets.
     */
    *start_frame_no = ((int)((h >> 16) & UINT64_C(0x07))) - 3;
    *enable_cms = (int)((h >> 32) & UINT64_C(1));
    *use_bgcolor = (int)((h >> 33) & UINT64_C(1));
    bgcolor[0] = (unsigned char)(h >> 40);
    bgcolor[1] = (unsigned char)(h >> 48);
    bgcolor[2] = (unsigned char)(h >> 56);
}

static SIXELSTATUS
fuzz_frame_callback(sixel_frame_t *frame, void *context)
{
    (void)context;
    if (frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    /*
     * Fuzz harness equivalent of --ignore-delay: do not preserve animation
     * pacing metadata between frames.
     */
    (void)sixel_frame_set_delay(frame, 0);
    return SIXEL_OK;
}

static void
fuzz_builtin_loader_dispose(void)
{
    if (g_component != NULL) {
        sixel_loader_component_unref(g_component);
        g_component = NULL;
    }
    if (g_factory != NULL) {
        loader_factory_unref(g_factory);
        g_factory = NULL;
    }
    if (g_allocator != NULL) {
        sixel_allocator_unref(g_allocator);
        g_allocator = NULL;
    }
    free(g_forced_chunk_buffer);
    g_forced_chunk_buffer = NULL;
    g_forced_chunk_capacity = 0u;

    g_fuzz_ready = 0;
}

static int
fuzz_builtin_loader_init(void)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (g_fuzz_ready) {
        return 1;
    }

    status = sixel_allocator_new(&g_allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fuzz_builtin_loader_dispose();
        return 0;
    }

    status = loader_factory_get_default(&g_factory);
    if (SIXEL_FAILED(status)) {
        fuzz_builtin_loader_dispose();
        return 0;
    }

    status = loader_factory_create_component(g_factory,
                                             "builtin",
                                             g_allocator,
                                             &g_component);
    if (SIXEL_FAILED(status)) {
        fuzz_builtin_loader_dispose();
        return 0;
    }

    g_fuzz_ready = 1;
    return 1;
}

int
LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;

    g_force_format = fuzz_parse_force_format(
        getenv("FUZZ_BUILTIN_FORCE_FORMAT"));

    if (fuzz_builtin_loader_init()) {
        (void)atexit(fuzz_builtin_loader_dispose);
    }

    return 0;
}

int
LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
    enum { FUZZ_MAX_INPUT_BYTES = 4 * 1024 * 1024 };

    sixel_chunk_t chunk;
    unsigned char const *payload_data;
    size_t payload_size;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    int loop_control;
    int start_frame_no;
    int enable_cms;
    int use_bgcolor;
    unsigned char bgcolor[3];

    if (size > (size_t)FUZZ_MAX_INPUT_BYTES) {
        return 0;
    }
    if (data == NULL) {
        return 0;
    }

    if (!g_fuzz_ready && !fuzz_builtin_loader_init()) {
        return 0;
    }
    if (g_component == NULL || g_allocator == NULL) {
        return 0;
    }

    payload_data = (unsigned char const *)(uintptr_t)data;
    payload_size = size;
    if (!fuzz_prepare_input_forced_format(data,
                                          size,
                                          &payload_data,
                                          &payload_size)) {
        return 0;
    }

    fuzz_pick_loader_options(payload_data, payload_size,
                             &fstatic,
                             &fuse_palette,
                             &reqcolors,
                             &loop_control,
                             &start_frame_no,
                             &enable_cms,
                             &use_bgcolor,
                             bgcolor);

    (void)sixel_loader_component_setopt(g_component,
                                        SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                        &fstatic);
    (void)sixel_loader_component_setopt(g_component,
                                        SIXEL_LOADER_OPTION_USE_PALETTE,
                                        &fuse_palette);
    (void)sixel_loader_component_setopt(g_component,
                                        SIXEL_LOADER_OPTION_REQCOLORS,
                                        &reqcolors);
    (void)sixel_loader_component_setopt(g_component,
                                        SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                        &loop_control);
    (void)sixel_loader_component_setopt(g_component,
                                        SIXEL_LOADER_OPTION_START_FRAME_NO,
                                        &start_frame_no);
    if (use_bgcolor) {
        (void)sixel_loader_component_setopt(g_component,
                                            SIXEL_LOADER_OPTION_BGCOLOR,
                                            bgcolor);
    } else {
        (void)sixel_loader_component_setopt(g_component,
                                            SIXEL_LOADER_OPTION_BGCOLOR,
                                            NULL);
    }
    sixel_helper_set_builtin_enable_cms(enable_cms);

    if (payload_size == 0u) {
        chunk.buffer = g_empty_input;
    } else {
        chunk.buffer = (unsigned char *)(uintptr_t)payload_data;
    }
    chunk.size = payload_size;
    chunk.max_size = payload_size;
    chunk.source_path = NULL;
    chunk.allocator = g_allocator;

    (void)sixel_loader_component_load(g_component,
                                      &chunk,
                                      fuzz_frame_callback,
                                      NULL);

    return 0;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
