/* SPDX-License-Identifier: MIT */
#include <math.h>
#include "tests/_support/loader/pixelformat_test_common.h"
#include "src/frame.h"
#include "tests/_support/loader/png_hdr_composition_common.h"
#include "tests/_support/loader/png_hdr_composition_expected.h"
#include "src/cms.h"
#include "src/pixelformat.h"
#include "src/loader-common.h"

static void
hdrp_be32(unsigned char *p, unsigned int value)
{
    p[0] = (unsigned char)(value >> 24);
    p[1] = (unsigned char)(value >> 16);
    p[2] = (unsigned char)(value >> 8);
    p[3] = (unsigned char)value;
}

static size_t
hdrp_chunk(unsigned char *p, char const *type,
             unsigned char const *bytes, size_t size)
{
    uint32_t crc;
    size_t i;
    unsigned int j;

    hdrp_be32(p, (unsigned int)size);
    memcpy(p + 4, type, 4u);
    if (size != 0u) {
        memcpy(p + 8, bytes, size);
    }
    crc = UINT32_C(0xffffffff);
    for (i = 4u; i < size + 8u; ++i) {
        crc ^= p[i];
        for (j = 0u; j < 8u; ++j) {
            crc = (crc >> 1) ^ ((crc & 1u) ? UINT32_C(0xedb88320) : 0u);
        }
    }
    hdrp_be32(p + size + 8u, crc ^ UINT32_C(0xffffffff));
    return size + 12u;
}

static size_t
hdrp_zlib(unsigned char *out, unsigned char const *in, size_t size)
{
    unsigned int a;
    unsigned int b;
    size_t i;

    out[0] = 0x78u;
    out[1] = 0x01u;
    out[2] = 1u;
    out[3] = (unsigned char)size;
    out[4] = (unsigned char)(size >> 8);
    out[5] = (unsigned char)~out[3];
    out[6] = (unsigned char)~out[4];
    memcpy(out + 7, in, size);
    a = 1u;
    b = 0u;
    for (i = 0u; i < size; ++i) {
        a = (a + in[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    hdrp_be32(out + 7u + size, (b << 16) | a);
    return size + 11u;
}

/* Build small lossless fixtures entirely in memory. All expected samples
 * live in the independent table; this builder only describes input bytes. */
static size_t
hdrp_image(unsigned char png[4096], int backend, int scenario, int option)
{
    static unsigned char const alpha[4] = {255u, 128u, 0u, 255u};
    unsigned char ihdr[13] = {0};
    unsigned char codes[4] = {9u, 16u, 0u, 1u};
    unsigned char background[6] = {0u, 64u, 0u, 96u, 0u, 192u};
    unsigned char animation[26];
    unsigned char raw[17];
    unsigned char compressed[64];
    size_t offset;
    size_t size;
    size_t f;
    size_t frames;
    size_t width;
    size_t x;
    size_t p;
    unsigned int sequence;
    unsigned char gray;

    memcpy(png, "\x89PNG\r\n\x1a\n", 8u);
    hdrp_be32(ihdr, 4u);
    hdrp_be32(ihdr + 4u, 1u);
    ihdr[8] = 8u;
    ihdr[9] = 6u;
    offset = 8u + hdrp_chunk(png + 8u, "IHDR", ihdr, sizeof(ihdr));
    frames = scenario == 2 ? 3u : 1u;
    codes[1] = scenario == 2 ? 18u : 16u;
    if (scenario != 1 || option == 0) {
        offset += hdrp_chunk(png + offset, "cICP", codes, sizeof(codes));
    }
    if (scenario == 1) {
        offset += hdrp_chunk(png + offset, "bKGD", background,
                             sizeof(background));
    }
    if (frames == 3u) {
        hdrp_be32(animation, 3u);
        hdrp_be32(animation + 4u, 1u);
        offset += hdrp_chunk(png + offset, "acTL", animation, 8u);
    }
    sequence = 0u;
    for (f = 0u; f < frames; ++f) {
        width = f == 0u ? 4u : f == 1u ? 2u : 1u;
        x = f;
        gray = f == 0u ? 128u : f == 1u ? 192u : 64u;
        if (frames == 3u) {
            memset(animation, 0, sizeof(animation));
            hdrp_be32(animation, sequence++);
            hdrp_be32(animation + 4u, (unsigned int)width);
            hdrp_be32(animation + 8u, 1u);
            hdrp_be32(animation + 12u, (unsigned int)x);
            animation[21] = 1u;
            animation[23] = 10u;
            animation[24] = (unsigned char)(f == 1u ? option : 0);
            animation[25] = 1u; /* OVER, including the first frame. */
            offset += hdrp_chunk(png + offset, "fcTL", animation, 26u);
        }
        raw[0] = 0u;
        for (p = 0u; p < width; ++p) {
            raw[1u + p * 4u] = gray;
            raw[2u + p * 4u] = gray;
            raw[3u + p * 4u] = gray;
            raw[4u + p * 4u] = frames == 3u ? 128u : alpha[p];
        }
        size = hdrp_zlib(compressed + 4u, raw, 1u + width * 4u);
        if (f == 0u) {
            offset += hdrp_chunk(png + offset, "IDAT", compressed + 4u, size);
        } else {
            hdrp_be32(compressed, sequence++);
            offset += hdrp_chunk(png + offset, "fdAT", compressed, size + 4u);
        }
    }
    offset += hdrp_chunk(png + offset, "IEND", NULL, 0u);
    if (backend == 2) {
        memmove(png + 54u, png, offset);
        memset(png, 0, 54u);
        png[0] = 'B';
        png[1] = 'M';
        png[2] = (unsigned char)(offset + 54u);
        png[3] = (unsigned char)((offset + 54u) >> 8);
        png[10] = 54u;
        png[14] = 40u;
        png[18] = 4u;
        png[22] = 1u;
        png[26] = 1u;
        png[30] = 5u; /* BI_PNG. */
        png[34] = (unsigned char)offset;
        png[35] = (unsigned char)(offset >> 8);
        offset += 54u;
    }
    return offset;
}

typedef struct hdrp_probe {
    double expected[3][4][3];
    int frames;
    int count;
    int preserve_zero;
} hdrp_probe_t;

static SIXELSTATUS
hdrp_capture(sixel_frame_t *frame, void *context)
{
    hdrp_probe_t *probe;
    sixel_frame_transparency_t alpha;
    float const *pixels;
    double expected;
    size_t p;
    size_t c;
    unsigned char mask;

    probe = context;
    if (probe->count >= probe->frames || sixel_frame_get_width(frame) != 4 ||
        sixel_frame_get_height(frame) != 1 ||
        sixel_frame_format(frame) != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
        || sixel_frame_get_colorspace(frame) != SIXEL_COLORSPACE_SRGB_LINEAR) {
        fprintf(stderr, "HDR frame dimensions/count/float representation\n");
        return SIXEL_BAD_INPUT;
    }
    pixels = (float const *)sixel_frame_get_pixels(frame);
    for (p = 0u; p < 4u; ++p) {
        for (c = 0u; c < 3u; ++c) {
            expected = probe->expected[probe->count][p][c];
            if (!isfinite(pixels[p * 3u + c]) ||
                fabs(pixels[p * 3u + c] - expected) > 2e-6) {
                fprintf(stderr, "HDR frame=%d pixel=%zu channel=%zu: "
                    "%.12g != %.12g\n", probe->count, p, c,
                    (double)pixels[p * 3u + c], expected);
                return SIXEL_BAD_INPUT;
            }
        }
    }
    memset(&alpha, 0, sizeof(alpha));
    if (SIXEL_FAILED(sixel_frame_as_interface(frame)->vtbl->get_transparency(
            sixel_frame_as_interface(frame), &alpha)) ||
        alpha.alpha_zero_is_transparent != probe->preserve_zero ||
        (probe->preserve_zero && alpha.transparent_mask_size != 4u)) {
        fprintf(stderr, "HDR zero-alpha publication differs\n");
        return SIXEL_BAD_INPUT;
    }
    if (alpha.transparent_mask_size != 0u) {
        if (alpha.transparent_mask == NULL ||
            alpha.transparent_mask_size != 4u) {
            return SIXEL_BAD_INPUT;
        }
        for (p = 0u; p < 4u; ++p) {
            mask = (unsigned char)(probe->preserve_zero && p == 2u);
            if (alpha.transparent_mask[p] != mask) {
                fprintf(stderr, "HDR zero mask differs at pixel %zu\n", p);
                return SIXEL_BAD_INPUT;
            }
        }
    }
    ++probe->count;
    return SIXEL_OK;
}

int
png_hdr_composition_test(int backend, int scenario, int option)
{
    unsigned char png[4096];
    unsigned char bg[3] = {51u, 102u, 153u};
    sixel_allocator_t *allocator;
    sixel_loader_component_t *component;
    sixel_chunk_t *chunk;
    sixel_cms_engine_t previous;
    loader_probe_callback_state_t callback;
    hdrp_probe_t probe;
    SIXELSTATUS status;
    size_t size;
    int previous_policy;
    int policy;
    int has_bg;
    int engine;
    int is_static;
    int one;
    int zero;
    int colors;
    int result;

#if !HAVE_LIBPNG
    if (backend == 1) {
        return 77;
    }
#endif
    allocator = NULL;
    component = NULL;
    chunk = NULL;
    previous = sixel_cms_get_engine();
    previous_policy = loader_transparent_policy();
    policy = scenario == 0 ? option / 2 : SIXEL_ALPHA_POLICY_COMPOSITE;
    has_bg = scenario == 2 || (scenario == 0 && option % 2 != 0);
    sixel_helper_set_loader_transparent_policy(policy);
    memset(&probe, 0, sizeof(probe));
    probe.frames = scenario == 2 ? 3 : 1;
    probe.preserve_zero = scenario == 0 && (!has_bg || policy != 0);
    if (scenario == 0) {
        memcpy(probe.expected[0], hdrp_alpha_expected[option],
               sizeof(probe.expected[0]));
    } else if (scenario == 1) {
        memcpy(probe.expected[0], hdrp_bkgd_expected,
               sizeof(probe.expected[0]));
    } else {
        memcpy(probe.expected, hdrp_apng_expected[option],
               sizeof(probe.expected));
    }
    engine = SIXEL_CMS_ENGINE_BUILTIN;
    is_static = scenario != 2;
    one = 1;
    zero = 0;
    colors = 256;
    result = 1;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    size = hdrp_image(png, backend, scenario, option);
    status = sixel_chunk_create_from_memory(&chunk, png, size, NULL, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = create_loader_component_by_name(
        backend == 1 ? "libpng" : "builtin", allocator, (void **)&component);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (SIXEL_FAILED(sixel_loader_component_setopt(component,
            SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE, &engine)) ||
        SIXEL_FAILED(sixel_loader_component_setopt(component,
            SIXEL_LOADER_OPTION_REQUIRE_STATIC, &is_static)) ||
        SIXEL_FAILED(sixel_loader_component_setopt(component,
            SIXEL_LOADER_OPTION_USE_PALETTE, &one)) ||
        SIXEL_FAILED(sixel_loader_component_setopt(component,
            SIXEL_LOADER_OPTION_REQCOLORS, &colors)) ||
        SIXEL_FAILED(sixel_loader_component_setopt(component,
            SIXEL_LOADER_COMPONENT_OPTION_PREFER_FLOAT32, &zero))) {
        goto cleanup;
    }
    if (has_bg && SIXEL_FAILED(sixel_loader_component_setopt(component,
            SIXEL_LOADER_OPTION_BGCOLOR, bg))) {
        goto cleanup;
    }
    callback = (loader_probe_callback_state_t){NULL, hdrp_capture, &probe};
    status = sixel_loader_component_load(component, chunk,
                                           capture_frame_trampoline, &callback);
    result = SIXEL_FAILED(status) || probe.count != probe.frames;
    if (result) {
        fprintf(stderr, "HDR backend=%d scenario=%d option=%d status=%x "
            "frames=%d/%d: %s\n", backend, scenario, option, status,
            probe.count, probe.frames, sixel_helper_get_additional_message());
    }
cleanup:
    sixel_helper_set_loader_transparent_policy(previous_policy);
    sixel_cms_set_engine(previous);
    sixel_loader_component_unref(component);
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return result;
}
