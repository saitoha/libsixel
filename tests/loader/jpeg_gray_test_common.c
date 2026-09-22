/* SPDX-License-Identifier: MIT */
/* Analytic JPEG samples and a gamma-two Gray ICC profile are independent
 * of the loader and CMS output. No runtime image encoder supplies an oracle. */
#include <math.h>
#include "tests/loader/pixelformat_test_common.h"
#include "tests/loader/builtin/loader_builtin_memory_test_common.h"
#include "jpeg_gray_test_common.h"
#include "src/cms.h"

#if HAVE_JPEG
typedef struct {
    edge_writer_t writer;
    unsigned int byte;
    int bits;
} jg_writer_t;

typedef struct {
    int depth;
    int engine;
    int mode;
    int calls;
} jg_probe_t;

/* DCT blocks are constant, with Q=1 and DC=8*(sample-center).
 * Lossless rows use predictor 1; adjacent 16-bit values expose byte rounding. */
static unsigned int const jg_samples8[] = {26, 64, 127, 128, 192, 253};
static unsigned int const jg_samples12[] = {257, 1023, 2047, 2049, 3073, 4033};
static unsigned int const jg_samples16[] = {
    257, 16383, 32767, 32768, 32769,
    258, 16384, 32769, 40001, 60001,
    259, 16385, 32770, 40002, 65534
};

static void
jg_marker(jg_writer_t *w, unsigned int marker)
{
    edge_put_u8(&w->writer, 255u);
    edge_put_u8(&w->writer, marker);
}

static void
jg_bits(jg_writer_t *w, unsigned int value, int count)
{
    int shift;

    for (shift = count - 1; shift >= 0; --shift) {
        w->byte = (w->byte << 1) | ((value >> shift) & 1u);
        if (++w->bits == 8) {
            edge_put_u8(&w->writer, w->byte);
            if (w->byte == 255u) {
                edge_put_u8(&w->writer, 0u);
            }
            w->bits = 0;
            w->byte = 0u;
        }
    }
}

static void
jg_difference(jg_writer_t *w, int difference)
{
    unsigned int magnitude;
    int category;

    magnitude = (unsigned int)abs(difference);
    category = 0;
    while (magnitude != 0u) {
        ++category;
        magnitude >>= 1;
    }
    jg_bits(w, (unsigned int)category, 5);
    jg_bits(w, difference < 0 ? (unsigned int)((1 << category) - 1 +
            difference) : (unsigned int)difference, category);
}

static size_t
jg_fixture(unsigned char *buffer, size_t capacity, int depth, int mode)
{
    unsigned char profile[4096];
    size_t profile_size;
    jg_writer_t w;
    edge_writer_t *out;
    int i;
    int value;
    int previous;
    int difference;

    memset(&w, 0, sizeof(w));
    w.writer.buffer = buffer;
    w.writer.capacity = capacity;
    out = &w.writer;
    jg_marker(&w, 0xd8u);
    if (mode != 4 && mode != 7) {
        if (edge_read_fixture("/tests/data/color-numeric/gray-gamma2.icc",
                profile, sizeof(profile), &profile_size) != 0) {
            return 0u;
        }
        if (mode == 5) {
            profile_size = 4u;
        }
        jg_marker(&w, 0xe2u);
        edge_put_u16be(out, (unsigned int)profile_size + 16u);
        edge_put_bytes(out, (unsigned char const *)"ICC_PROFILE", 12u);
        edge_put_u8(out, 1u);
        edge_put_u8(out, 1u);
        edge_put_bytes(out, profile, profile_size);
    }
    jg_marker(&w, depth == 16 ? 0xc3u : depth == 12 ? 0xc1u : 0xc0u);
    edge_put_u16be(out, 11u);
    edge_put_u8(out, (unsigned int)depth);
    edge_put_u16be(out, depth == 16 ? 3u : 9u);
    edge_put_u16be(out, depth == 16 ? 5u : 17u);
    edge_put_u8(out, 1u);
    edge_put_u8(out, 1u);
    edge_put_u8(out, 0x11u);
    edge_put_u8(out, 0u);
    if (depth != 16) {
        jg_marker(&w, 0xdbu);
        edge_put_u16be(out, 67u);
        edge_put_u8(out, 0u);
        for (i = 0; i < 64; ++i) {
            edge_put_u8(out, 1u);
        }
    }
    /* Fixed five-bit Huffman codes cover DC categories 0..15, leaving
     * the all-ones code unused. Each DCT block ends with AC code 0 (EOB). */
    jg_marker(&w, 0xc4u);
    edge_put_u16be(out, 35u);
    edge_put_u8(out, 0u);
    for (i = 0; i < 16; ++i) {
        edge_put_u8(out, i == 4 ? 16u : 0u);
    }
    for (i = 0; i < 16; ++i) {
        edge_put_u8(out, (unsigned int)i);
    }
    if (depth != 16) {
        jg_marker(&w, 0xc4u);
        edge_put_u16be(out, 20u);
        edge_put_u8(out, 16u);
        for (i = 0; i < 16; ++i) {
            edge_put_u8(out, i == 0 ? 1u : 0u);
        }
        edge_put_u8(out, 0u);
    }
    jg_marker(&w, 0xdau);
    edge_put_u16be(out, 8u);
    edge_put_u8(out, 1u);
    edge_put_u8(out, 1u);
    edge_put_u8(out, 0u);
    edge_put_u8(out, depth == 16 ? 1u : 0u);
    edge_put_u8(out, depth == 16 ? 0u : 63u);
    edge_put_u8(out, 0u);
    previous = 0;
    for (i = 0; i < (depth == 16 ? 15 : 6); ++i) {
        if (depth == 16) {
            value = (int)jg_samples16[i];
            previous = i == 0 ? 32768 :
                (int)jg_samples16[i % 5 == 0 ? i - 5 : i - 1];
        } else {
            value = 8 * ((int)(depth == 12 ? jg_samples12[i] :
                              jg_samples8[i]) - (1 << (depth - 1)));
        }
        difference = value - previous;
        jg_difference(&w, difference);
        if (depth != 16) {
            jg_bits(&w, 0u, 1);
        }
        previous = value;
    }
    while (w.bits != 0) {
        jg_bits(&w, 1u, 1);
    }
    jg_marker(&w, 0xd9u);
    return out->failed ? 0u : out->length;
}

static SIXELSTATUS
jg_capture(sixel_frame_t *frame, void *context)
{
    jg_probe_t *p;
    unsigned char const *bytes;
    float const *pixels;
    unsigned int sample;
    double source;
    double expected;
    double actual;
    double bound;
    size_t i;
    size_t c;
    int slot;
    int byte_frame;
    int gamma_frame;
    int managed;
    int width;
    int height;

    p = context;
    ++p->calls;
    width = p->depth == 16 ? 5 : 17;
    height = p->depth == 16 ? 3 : 9;
    byte_frame = p->mode == 3 || p->mode == 4;
    gamma_frame = byte_frame || p->mode == 6;
    managed = p->mode == 0;
    if (frame->width != width || frame->height != height ||
        frame->pixelformat != (byte_frame ? SIXEL_PIXELFORMAT_RGB888 :
            gamma_frame ? SIXEL_PIXELFORMAT_RGBFLOAT32 :
                          SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) ||
        frame->colorspace != (gamma_frame ? SIXEL_COLORSPACE_GAMMA :
                                           SIXEL_COLORSPACE_LINEAR) ||
        frame->palette != NULL || frame->transparent_mask != NULL) {
        fprintf(stderr, "JGRAY metadata format=%x space=%d\n",
                frame->pixelformat, frame->colorspace);
        return SIXEL_BAD_INPUT;
    }
    bytes = sixel_frame_get_pixels(frame);
    pixels = (float const *)bytes;
    for (i = 0u; i < (size_t)width * (size_t)height; ++i) {
        slot = (int)(i / (size_t)width / 8u * 3u + i % (size_t)width / 8u);
        sample = p->depth == 16 ? jg_samples16[i] :
                 p->depth == 12 ? jg_samples12[slot] : jg_samples8[slot];
        source = (double)sample / (double)((1u << p->depth) - 1u);
        /* kTRC is exactly gamma 2. D50 neutral PCS maps to equal RGB.
         * Fallback uses the documented sRGB transfer on original samples. */
        expected = managed ? source * source :
            gamma_frame ? source : source <= 0.04045 ? source / 12.92 :
                            pow((source + 0.055) / 1.055, 2.4);
        bound = p->engine == SIXEL_CMS_ENGINE_COLORSYNC && managed
              ? 0.0001 : 0.000004 * fabs(expected) + 0.0000001;
        if (byte_frame) {
            expected = floor(expected * 255.0 + 0.5);
            bound = 0.0;
        } else if (p->mode == 6) {
            /* Gray12 uses a rounded float reciprocal and multiplication;
             * two roundings allow 2*u32 relative to the exact source ratio.
             * Native Gray16 divides directly and must match the float cast. */
            expected = p->depth == 12 ? source : (float)source;
            bound = p->depth == 12 ? source * 0x1p-23 : 0.0;
        }
        for (c = 0u; c < 3u; ++c) {
            actual = byte_frame ? bytes[i * 3u + c] : pixels[i * 3u + c];
            if (!isfinite(actual) || fabs(actual - expected) > bound) {
                fprintf(stderr, "JGRAY sample=%u c=%zu got=%.12g "
                    "expected=%.12g bound=%.9g\n", sample, c,
                    actual, expected, bound);
                return SIXEL_BAD_INPUT;
            }
        }
    }
    return SIXEL_OK;
}
#endif

int
jpeg_gray_values(int depth, int engine, int mode)
{
#if HAVE_JPEG
    unsigned char fixture[8192];
    size_t size;
    jg_probe_t probe;
    sixel_allocator_t *allocator;
    sixel_loader_t *loader;
    sixel_chunk_t *chunk;
    sixel_cms_engine_t previous;
    SIXELSTATUS status;
    char order[128];
    int n;

#if !HAVE_JPEG12_API
    if (depth == 12) { return 77; }
#endif
#if !HAVE_JPEG16_API
    if (depth == 16) { return 77; }
#endif
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    n = (int)sixel_cms_get_engine() == engine;
    sixel_cms_set_engine(previous);
    if (!n) { return 77; }
    size = jg_fixture(fixture, sizeof(fixture), depth, mode);
    if (size == 0u) { return 1; }
    probe.depth = depth;
    probe.engine = engine;
    probe.mode = mode;
    probe.calls = 0;
    loader = NULL;
    chunk = NULL;
    allocator = NULL;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) { goto end; }
    status = sixel_chunk_create_from_memory(&chunk, fixture, size, NULL,
                                            allocator);
    if (SIXEL_FAILED(status)) { goto end; }
    status = sixel_loader_new(&loader, NULL);
    if (SIXEL_FAILED(status)) { goto end; }
    n = snprintf(order, sizeof(order),
        "libjpeg:cms_engine=%s!",
        sixel_cms_engine_to_string((sixel_cms_engine_t)engine));
    if (n < 0 || (size_t)n >= sizeof(order)) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
                                 order);
    if (SIXEL_FAILED(status)) { goto end; }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT, &probe);
    if (SIXEL_FAILED(status)) { goto end; }
    sixel_loader_set_prefer_float32(loader, mode == 6 || mode == 7);
    status = sixel_loader_load_chunk(loader, chunk, jg_capture);
end:
    sixel_loader_unref(loader);
    if (chunk != NULL) { chunk->vtbl->unref(chunk); }
    sixel_allocator_unref(allocator);
    sixel_cms_set_engine(previous);
    if (SIXEL_FAILED(status) || probe.calls != 1) {
        fprintf(stderr, "JGRAY depth=%d engine=%d mode=%d status=%x: %s\n",
                depth, engine, mode, status,
                sixel_helper_get_additional_message());
        return 1;
    }
    return 0;
#else
    (void)depth;
    (void)engine;
    (void)mode;
    return 77;
#endif
}
