/* SPDX-License-Identifier: MIT */
/* Original PLTE samples use the existing independent PNG matrix/TRC oracle. */
#include <math.h>
#include <stdint.h>
#include "tests/_support/loader/pixelformat_test_common.h"
#include "tests/_support/loader/builtin/loader_builtin_memory_test_common.h"
#include "tests/data/color-numeric/png-samples.h"
#include "src/cms.h"
#include "src/loader-common.h"

#if HAVE_LIBPNG
#define IP_WIDTH 5
#define IP_HEIGHT 3
#define IP_FLOAT_BYTES (IP_WIDTH * IP_HEIGHT * 3u * sizeof(float))

typedef struct {
    unsigned char bytes[4096];
    size_t size;
} ip_png_t;

typedef struct {
    int variant;
    int bits;
    int colors;
    int managed;
    int target;
    int calls;
    double tolerance;
} ip_probe_t;

static int ip_live;
static int ip_fail_float;
static int ip_float_allocations;

static void *
ip_malloc(size_t size)
{
    void *p;

    if (size == IP_FLOAT_BYTES) {
        ++ip_float_allocations;
        if (ip_fail_float) {
            ip_fail_float = 0;
            return NULL;
        }
    }
    p = malloc(size);
    if (p != NULL) {
        ++ip_live;
    }
    return p;
}

static void
ip_free(void *p)
{
    if (p != NULL) {
        --ip_live;
    }
    free(p);
}

static void *
ip_calloc(size_t n, size_t size)
{
    void *p;

    if (size != 0u && n > SIZE_MAX / size) {
        return NULL;
    }
    p = ip_malloc(n * size);
    if (p != NULL) {
        memset(p, 0, n * size);
    }
    return p;
}

static void *
ip_realloc(void *p, size_t size)
{
    void *q;
    int was_null;

    if (size == 0u) {
        ip_free(p);
        return NULL;
    }
    was_null = p == NULL;
    q = realloc(p, size);
    if (q != NULL && was_null) {
        ++ip_live;
    }
    return q;
}

static void
ip_u32(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)(value >> 24);
    p[1] = (unsigned char)(value >> 16);
    p[2] = (unsigned char)(value >> 8);
    p[3] = (unsigned char)value;
}

static void
ip_chunk(ip_png_t *f, char const *type,
          unsigned char const *data, size_t size)
{
    unsigned char *p;
    uint32_t crc;
    size_t i;
    int bit;

    if (size + 12u > sizeof(f->bytes) - f->size) {
        abort();
    }
    p = f->bytes + f->size;
    ip_u32(p, (uint32_t)size);
    memcpy(p + 4, type, 4);
    if (size != 0u) {
        memcpy(p + 8, data, size);
    }
    crc = UINT32_C(0xffffffff);
    for (i = 4u; i < size + 8u; ++i) {
        crc ^= p[i];
        for (bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1u) ? UINT32_C(0xedb88320) : 0u);
        }
    }
    ip_u32(p + size + 8u, ~crc);
    f->size += size + 12u;
}

static int
ip_fixture(ip_png_t *f, ip_probe_t const *probe)
{
    /* Adam7 pass starts/steps; samples always follow the same image map. */
    static int const passes[7][4] = {
        {0, 0, 8, 8}, {4, 0, 8, 8}, {0, 4, 4, 8}, {2, 0, 4, 4},
        {0, 2, 2, 4}, {1, 0, 2, 2}, {0, 1, 1, 2}
    };
    static uint32_t const chrm[8] = {
        31270, 32900, 67000, 32000, 26500, 69000, 15000, 6000
    };
    unsigned char metadata[4096];
    unsigned char header[13];
    unsigned char palette[24];
    unsigned char raw[128];
    unsigned char data[160];
    unsigned char xy[32];
    unsigned char alpha[8];
    size_t metadata_size;
    size_t offset;
    size_t length;
    size_t name;
    size_t used;
    size_t row;
    size_t i;
    uint32_t a;
    uint32_t b;
    int pass;
    int x;
    int y;
    int first_x;
    int first_y;
    int step_x;
    int step_y;
    int column;
    int slot;
    int interlaced;
    int icc;

    interlaced = probe->variant == 5;
    icc = probe->variant != 6 && probe->variant != 7 &&
          probe->variant != 9 && probe->variant != 20;
    memcpy(f->bytes, "\211PNG\r\n\032\n", 8);
    f->size = 8u;
    memset(header, 0, sizeof(header));
    ip_u32(header, IP_WIDTH);
    ip_u32(header + 4, IP_HEIGHT);
    header[8] = (unsigned char)probe->bits;
    header[9] = 3u;
    header[12] = (unsigned char)interlaced;
    ip_chunk(f, "IHDR", header, sizeof(header));
    if (icc) {
        if (edge_read_fixture("/tests/data/color-numeric/png-p3-8-rgb.png",
            metadata, sizeof(metadata), &metadata_size) != 0) {
            return 1;
        }
        for (offset = 8u; offset + 12u <= metadata_size;
             offset += length + 12u) {
            length = ((size_t)metadata[offset] << 24) |
                ((size_t)metadata[offset + 1u] << 16) |
                ((size_t)metadata[offset + 2u] << 8) | metadata[offset + 3u];
            if (length > metadata_size - offset - 12u) {
                return 1;
            }
            if (memcmp(metadata + offset + 4u, "iCCP", 4u) == 0) {
                if (probe->variant == 10 || probe->variant == 11) {
                    /* Getter-rejected profile; lower metadata still applies. */
                    name = 0u;
                    while (name < length && metadata[offset + 8u + name]) {
                        ++name;
                    }
                    if (name + 1u >= length) {
                        return 1;
                    }
                    metadata[offset + 8u + name + 1u] = 1u;
                }
                ip_chunk(f, "iCCP", metadata + offset + 8u, length);
                break;
            }
        }
        if (offset + 12u > metadata_size) {
            return 1;
        }
    }
    if (probe->variant == 6) {
        for (i = 0u; i < 8u; ++i) {
            ip_u32(xy + i * 4u, chrm[i]);
        }
        ip_chunk(f, "cHRM", xy, sizeof(xy));
    }
    if (probe->variant == 7 || probe->variant == 10) {
        ip_u32(data, 50000u);
        ip_chunk(f, "gAMA", data, 4u);
    }
    if (probe->variant == 20) {
        data[0] = 0u;
        ip_chunk(f, "sRGB", data, 1u);
    }
    for (slot = 0; slot < probe->colors; ++slot) {
        for (i = 0u; i < 3u; ++i) {
            palette[slot * 3 + i] =
                (unsigned char)color_png_source[0][slot][i];
        }
        alpha[slot] = (unsigned char)color_png_source[0][slot][3];
    }
    ip_chunk(f, "PLTE", palette, (size_t)probe->colors * 3u);
    if (probe->variant == 17) {
        ip_chunk(f, "tRNS", alpha, (size_t)probe->colors);
    }
    used = 0u;
    memset(raw, 0, sizeof(raw));
    for (pass = 0; pass < (interlaced ? 7 : 1); ++pass) {
        first_x = interlaced ? passes[pass][0] : 0;
        first_y = interlaced ? passes[pass][1] : 0;
        step_x = interlaced ? passes[pass][2] : 1;
        step_y = interlaced ? passes[pass][3] : 1;
        if (first_x >= IP_WIDTH || first_y >= IP_HEIGHT) {
            continue;
        }
        for (y = first_y; y < IP_HEIGHT; y += step_y) {
            ++used; /* Filter type zero for this pass row. */
            row = used;
            column = 0;
            for (x = first_x; x < IP_WIDTH; x += step_x) {
                slot = (y * IP_WIDTH + x) % probe->colors;
                if (probe->variant == 18 && x == 0 && y == 0) {
                    slot = 15; /* Invalid PLTE index, still fits the sample. */
                }
                raw[row + (size_t)(column * probe->bits / 8)] |=
                    (unsigned char)(slot <<
                        (8 - probe->bits - column * probe->bits % 8));
                ++column;
            }
            used += (size_t)(column * probe->bits + 7) / 8u;
        }
    }
    /* Stored DEFLATE keeps the fixture independent of an image encoder. */
    data[0] = 0x78u;
    data[1] = 1u;
    data[2] = 1u;
    data[3] = (unsigned char)used;
    data[4] = 0u;
    data[5] = (unsigned char)~used;
    data[6] = 255u;
    memcpy(data + 7, raw, used);
    a = 1u;
    b = 0u;
    for (i = 0u; i < used; ++i) {
        a = (a + raw[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    ip_u32(data + 7u + used, (b << 16) | a);
    ip_chunk(f, "IDAT", data, used + 11u);
    ip_chunk(f, "IEND", NULL, 0u);
    return 0;
}

static SIXELSTATUS
ip_capture(sixel_frame_t *frame, void *context)
{
    ip_probe_t *probe;
    unsigned char const *bytes;
    float const *pixels;
    double expected;
    double actual;
    double magnitude;
    int format;
    int slot;
    int p;
    int c;
    int index;
    int bits;

    probe = context;
    ++probe->calls;
    format = frame->pixelformat;
    if (frame->width != IP_WIDTH || frame->height != IP_HEIGHT) {
        return SIXEL_BAD_INPUT;
    }
    bytes = sixel_frame_get_pixels(frame);
    if (!probe->managed) {
        if ((format & SIXEL_FORMATTYPE_PALETTE) == 0 ||
            frame->colorspace != SIXEL_COLORSPACE_GAMMA ||
            frame->ncolors != probe->colors || frame->palette == NULL ||
            ip_float_allocations != 0) {
            fprintf(stderr, "unmanaged indexed storage was promoted\n");
            return SIXEL_BAD_INPUT;
        }
        bits = format == SIXEL_PIXELFORMAT_PAL8 ? 8 : probe->bits;
        for (p = 0; p < IP_WIDTH * IP_HEIGHT; ++p) {
            index = bytes[(p / IP_WIDTH) * ((IP_WIDTH * bits + 7) / 8) +
                          (p % IP_WIDTH) * bits / 8];
            index = (index >> (8 - bits - (p % IP_WIDTH) * bits % 8)) &
                    ((1 << bits) - 1);
            if (index != p % probe->colors) {
                return SIXEL_BAD_INPUT;
            }
        }
        for (slot = 0; slot < probe->colors; ++slot) {
            for (c = 0; c < 3; ++c) {
                if (frame->palette[slot * 3 + c] !=
                    color_png_source[0][slot][c]) {
                    return SIXEL_BAD_INPUT;
                }
            }
        }
        return SIXEL_OK;
    }
    /* Pre-expanded RGB retains a legacy source color count, but has no
     * palette buffer. The newly promoted indexed path clears both fields. */
    if (format != (probe->target == 2 ? SIXEL_PIXELFORMAT_RGB888 :
                   probe->target == 1 ? SIXEL_PIXELFORMAT_RGBFLOAT32 :
                   SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) ||
        frame->colorspace != (probe->target ? SIXEL_COLORSPACE_GAMMA :
                                             SIXEL_COLORSPACE_LINEAR) ||
        frame->palette != NULL ||
        (probe->variant != 16 && frame->ncolors > 0)) {
        fprintf(stderr, "managed indexed format=%x colors=%d\n", format,
                frame->ncolors);
        return SIXEL_BAD_INPUT;
    }
    pixels = (float const *)bytes;
    for (p = 0; p < IP_WIDTH * IP_HEIGHT; ++p) {
        slot = p % probe->colors;
        if (probe->variant == 17 &&
            (frame->transparent_mask_size != IP_WIDTH * IP_HEIGHT ||
             frame->transparent_mask == NULL ||
             !!frame->transparent_mask[p] !=
                (color_png_source[0][slot][3] == 0u))) {
            return SIXEL_BAD_INPUT;
        }
        for (c = 0; c < 3; ++c) {
            expected = color_png_icc_linear[0][slot][c];
            if (probe->variant == 6) {
                expected = color_png_wide_chrm_linear[0][slot][c];
            } else if (probe->variant == 7 || probe->variant == 10) {
                expected = color_png_source[0][slot][c] / 255.0;
                expected *= expected;
            }
            if (probe->target) {
                magnitude = fabs(expected);
                expected = copysign(magnitude <= 0.0031308
                    ? magnitude * 12.92
                    : 1.055 * pow(magnitude, 1.0 / 2.4) - 0.055, expected);
            }
            if (probe->target == 2) {
                expected = floor(fmax(0.0, fmin(1.0, expected)) *
                                   255.0 + 0.5);
                actual = bytes[p * 3 + c];
            } else {
                actual = pixels[p * 3 + c];
            }
            if (!isfinite(actual) || fabs(actual - expected) >
                (probe->target == 2 ? 0.0 : probe->tolerance)) {
                fprintf(stderr, "indexed p=%d c=%d: %.10g != %.10g\n",
                        p, c, actual, expected);
                return SIXEL_BAD_INPUT;
            }
        }
    }
    return SIXEL_OK;
}
#endif

int
test_libpng_indexed_precision(int argc, char **argv)
{
#if HAVE_LIBPNG
    ip_png_t fixture;
    ip_probe_t probe;
    sixel_loader_t *loader;
    sixel_chunk_t *chunk;
    sixel_allocator_t *allocator;
    sixel_cms_engine_t engine;
    sixel_cms_engine_t previous;
    SIXELSTATUS status;
    char order[128];
    int use_palette;
    int colors;
    int n;

    if (argc != 2) {
        return 1;
    }
    memset(&probe, 0, sizeof(probe));
    probe.variant = atoi(argv[1]);
    if (probe.variant < 1 || probe.variant > 20) {
        return 1;
    }
    probe.bits = probe.variant == 2 ? 1 : probe.variant == 3 ? 2 :
                 probe.variant == 4 || probe.variant == 18 ? 4 : 8;
    probe.colors = probe.bits < 4 ? 1 << probe.bits : 8;
    probe.managed = probe.variant != 8 && probe.variant != 9 &&
                    probe.variant != 11 && probe.variant != 20;
    probe.target = probe.variant == 12 ? 1 : probe.variant == 13 ? 2 : 0;
    engine = probe.variant == 8 ? SIXEL_CMS_ENGINE_NONE :
             probe.variant == 14 ? SIXEL_CMS_ENGINE_LCMS2 :
             probe.variant == 15 ? SIXEL_CMS_ENGINE_COLORSYNC :
             SIXEL_CMS_ENGINE_BUILTIN;
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(engine);
    if (sixel_cms_get_engine() != engine) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    probe.tolerance = engine == SIXEL_CMS_ENGINE_COLORSYNC ? 0.0001 :
                      probe.target ? 0.000003 : 0.0000005;
    if (ip_fixture(&fixture, &probe) != 0) {
        sixel_cms_set_engine(previous);
        return 1;
    }
    allocator = NULL;
    chunk = NULL;
    loader = NULL;
    use_palette = probe.variant == 16 ? 0 : 1;
    colors = 256;
    ip_live = 0;
    ip_float_allocations = 0;
    ip_fail_float = 0;
    status = sixel_allocator_new(&allocator, ip_malloc, ip_calloc,
                                  ip_realloc, ip_free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_chunk_create_from_memory(&chunk, fixture.bytes,
                                            fixture.size, NULL, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_new(&loader, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    n = snprintf(order, sizeof(order),
        "libpng:cms_engine=%s:cms_target=%s:prefer_8bit=%d!",
        sixel_cms_engine_to_string(engine), probe.target ? "gamma" : "linear",
        probe.target == 2);
    if (n < 0 || (size_t)n >= sizeof(order)) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
                                 order);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    (void)sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT, &probe);
    (void)sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_USE_PALETTE,
                               &use_palette);
    (void)sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_REQCOLORS, &colors);
    /* A byte preference is not permission to truncate applied CMS output. */
    sixel_loader_set_prefer_float32(loader, 0);
    ip_float_allocations = 0;
    ip_fail_float = probe.variant == 19;
    status = sixel_loader_load_chunk(loader, chunk, ip_capture);
    if (probe.variant == 18 || probe.variant == 19) {
        if (SIXEL_SUCCEEDED(status) || probe.calls != 0 ||
            (probe.variant == 19 && (ip_fail_float ||
                                     ip_float_allocations != 1))) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (probe.variant == 18) {
            status = SIXEL_OK;
            goto end;
        }
        /* Reuse the same loader after the failed expansion allocation. */
        status = sixel_loader_load_chunk(loader, chunk, ip_capture);
    }
    if (SIXEL_SUCCEEDED(status) && probe.calls != 1) {
        status = SIXEL_BAD_INPUT;
    }
end:
    ip_fail_float = 0;
    sixel_loader_unref(loader);
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_allocator_unref(allocator);
    sixel_cms_set_engine(previous);
    if (SIXEL_FAILED(status) || ip_live != 0) {
        fprintf(stderr, "indexed variant=%d status=%x calls=%d live=%d\n",
                probe.variant, status, probe.calls, ip_live);
        return 1;
    }
    puts("indexed-values-verified");
    return 0;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
