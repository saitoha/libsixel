/* SPDX-License-Identifier: MIT */
/* Four pixels, in-memory PNG/Adam7/APNG construction, independent analytic
 * expected samples. ICC export builds only a conflicting lower-priority
 * declaration, never the P3 reference values. */
#include <math.h>
#include "tests/_support/loader/pixelformat_test_common.h"
#include "src/cms.h"
#include "src/loader-common.h"
#include "tests/_support/loader/png_cicp_test_common.h"

typedef struct cicp_probe {
    int count;
    int format;
    int colorspace;
    int palette;
    int alpha_zero;
    unsigned char mask[4];
    unsigned char bytes[12];
    float values[12];
} cicp_probe_t;

static void
cicp_be32(unsigned char *p, unsigned int value)
{
    p[0] = (unsigned char)(value >> 24);
    p[1] = (unsigned char)(value >> 16);
    p[2] = (unsigned char)(value >> 8);
    p[3] = (unsigned char)value;
}

static size_t
cicp_chunk(unsigned char *p, char const *type,
             unsigned char const *bytes, size_t size)
{
    uint32_t crc;
    size_t i;
    unsigned int j;

    cicp_be32(p, (unsigned int)size);
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
    cicp_be32(p + size + 8u, crc ^ UINT32_C(0xffffffff));
    return size + 12u;
}

static size_t
cicp_zlib(unsigned char *out, unsigned char const *in, size_t size)
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
    cicp_be32(out + 7u + size, (b << 16) | a);
    return size + 11u;
}

static SIXELSTATUS
cicp_capture(sixel_frame_t *frame, void *context)
{
    cicp_probe_t *probe;
    sixel_frame_transparency_t alpha;
    SIXELSTATUS status;

    probe = context;
    if (sixel_frame_get_width(frame) != 4 ||
        sixel_frame_get_height(frame) != 1) {
        return SIXEL_BAD_INPUT;
    }
    ++probe->count;
    probe->format = sixel_frame_get_pixelformat(frame);
    probe->colorspace = sixel_frame_get_colorspace(frame);
    probe->palette = (probe->format & SIXEL_FORMATTYPE_PALETTE) != 0;
    if (probe->palette && sixel_frame_get_transparent(frame) >= 0) {
        /* Retained byte palettes expose a key, rather than a zero mask. */
        probe->alpha_zero = 1;
    }
    if (probe->palette) {
        status = sixel_frame_set_pixelformat(frame, SIXEL_PIXELFORMAT_RGB888);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    if (probe->format == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        probe->format == SIXEL_PIXELFORMAT_RGBFLOAT32) {
        memcpy(probe->values, sixel_frame_get_pixels(frame),
               sizeof(probe->values));
    } else {
        if (sixel_frame_get_pixelformat(frame) == SIXEL_PIXELFORMAT_G8) {
            return SIXEL_BAD_INPUT;
        }
        memcpy(probe->bytes, sixel_frame_get_pixels(frame),
               sizeof(probe->bytes));
    }
    memset(&alpha, 0, sizeof(alpha));
    status = sixel_frame_as_interface(frame)->vtbl->get_transparency(
        sixel_frame_as_interface(frame), &alpha);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    probe->alpha_zero |= alpha.alpha_zero_is_transparent;
    if (alpha.transparent_mask_size == 4u) {
        memcpy(probe->mask, alpha.transparent_mask, 4u);
    }
    return SIXEL_OK;
}

static double
cicp_decode(double x)
{
    return x <= 0.04045 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

static double
cicp_encode(double x)
{
    return copysign(fabs(x) <= 0.0031308 ? fabs(x) * 12.92 :
        1.055 * pow(fabs(x), 1.0 / 2.4) - 0.055, x);
}

int
png_cicp_test(int specimen, int native)
{
    /* Derived from P3 and sRGB xy primaries at D65: inverse(MsRGB)*MP3.
     * No loader/CMS/profile-generated values enter this expectation. */
    static double const matrix[3][3] = {
        {1.2249401762805598, -0.2249401762805598, 0.0},
        {-0.0420569547096882, 1.0420569547096882, 0.0},
        {-0.0196375545903344, -0.0786360455506319, 1.0982736001409663}
    };
    static double const xy[8] = {
        0.3127, 0.3290, 0.64, 0.33, 0.30, 0.60, 0.15, 0.06
    };
    static double const gamma[3] = {1.0, 1.0, 1.0};
    static unsigned int const source[4][4] = {
        {65535u, 0u, 0u, 65535u}, {0u, 65535u, 0u, 32769u},
        {0u, 0u, 65535u, 0u}, {8193u, 32769u, 49153u, 65535u}
    };
    static unsigned int const start[7] = {0u, 4u, 0u, 2u, 0u, 1u, 0u};
    static unsigned int const step[7] = {8u, 8u, 4u, 4u, 2u, 2u, 1u};
    unsigned char png[8192];
    unsigned char ihdr[13];
    unsigned char codes[4];
    unsigned char raw[128];
    unsigned char compressed[2048];
    unsigned char metadata[2048];
    unsigned char palette[12];
    unsigned char background[6];
    unsigned char animation[26];
    unsigned char *icc;
    unsigned char bg[3];
    double input[4][3];
    double expected[4][3];
    double lin[3];
    double value;
    double alpha;
    double tolerance;
    size_t offset;
    size_t raw_size;
    size_t compressed_size;
    size_t icc_size;
    size_t i;
    size_t c;
    size_t k;
    unsigned int sample;
    unsigned int pass;
    int indexed;
    int gray;
    int depth;
    int channels;
    int has_alpha;
    int adam7;
    int apng;
    int srgb;
    int unmanaged;
    int use_icc;
    int use_lower;
    int byte_expected;
    int one;
    int zero;
    int colors;
    int policy;
    int engine;
    int result;
    SIXELSTATUS status;
    sixel_cms_engine_t previous;
    sixel_allocator_t *allocator;
    sixel_loader_component_t *component;
    sixel_color_def_t *definition;
    sixel_chunk_t *chunk;
    loader_probe_callback_state_t callback;
    cicp_probe_t probe;

#if !HAVE_LIBPNG
    if (native) {
        return 77;
    }
#endif
    engine = specimen == 27 ? SIXEL_CMS_ENGINE_LCMS2 :
        specimen == 28 ? SIXEL_CMS_ENGINE_COLORSYNC : SIXEL_CMS_ENGINE_BUILTIN;
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    result = (int)sixel_cms_get_engine() == engine;
    sixel_cms_set_engine(previous);
    if (!result) {
        return 77;
    }
    depth = specimen == 2 || specimen == 4 || specimen == 7 ||
        specimen == 8 || specimen == 30 ? 16 : 8;
    indexed = specimen == 5 || specimen == 15 || specimen == 23 ||
        specimen == 32 || specimen == 33;
    gray = specimen == 6 || specimen == 7 || specimen == 8;
    has_alpha = specimen == 3 || specimen == 4 || specimen == 8 ||
        specimen == 9 || specimen == 10 || specimen == 30 ||
        specimen == 32 || specimen == 33;
    adam7 = specimen == 31;
    apng = specimen == 29;
    srgb = (specimen >= 13 && specimen <= 15) || specimen == 33;
    unmanaged = specimen == 25;
    use_icc = (specimen >= 11 && specimen <= 13) || specimen == 16;
    use_lower = specimen == 12 || specimen == 17;
    byte_expected = srgb || (specimen >= 18 && specimen <= 25);
    channels = (gray ? 1 : 3) + has_alpha;
    allocator = NULL;
    component = NULL;
    definition = NULL;
    chunk = NULL;
    icc = NULL;
    icc_size = 0u;
    result = 1;
    one = 1;
    zero = 0;
    colors = 256;
    policy = loader_transparent_policy();
    sixel_helper_set_loader_transparent_policy(SIXEL_ALPHA_POLICY_COMPOSITE);
    memset(&probe, 0, sizeof(probe));
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    memset(ihdr, 0, sizeof(ihdr));
    cicp_be32(ihdr, 4u);
    cicp_be32(ihdr + 4, 1u);
    ihdr[8] = (unsigned char)(indexed ? 2 : depth);
    ihdr[9] = (unsigned char)(indexed ? 3 : (gray ? 0 : 2) + has_alpha * 4);
    ihdr[12] = (unsigned char)adam7;
    memcpy(png, "\x89PNG\r\n\x1a\n", 8u);
    offset = 8u + cicp_chunk(png + 8, "IHDR", ihdr, sizeof(ihdr));
    codes[0] = (unsigned char)(srgb ? 1u : 12u);
    codes[1] = (unsigned char)(specimen == 16 ? 16u : 13u);
    codes[2] = (unsigned char)(specimen == 19 ? 1u : 0u);
    codes[3] = (unsigned char)(specimen == 18 ? 0u : 1u);
    if (specimen == 17) {
        codes[0] = 255u;
    }
    if (specimen != 23 && specimen != 24) {
        offset += cicp_chunk(png + offset, "cICP", codes,
                             specimen == 21 ? 3u : 4u);
        if (specimen == 20) {
            png[offset - 1u] ^= 1u;
        }
        if (specimen == 22) {
            offset += cicp_chunk(png + offset, "cICP", codes, 4u);
        }
    }
    if (use_icc) {
        definition = sixel_cdef_rgb(xy, SIXEL_COLOR_TRC_POWER, gamma);
        if (definition == NULL || SIXEL_FAILED(sixel_cdef_export_icc(
                definition, &icc, &icc_size, allocator)) || icc_size > 1900u) {
            goto cleanup;
        }
        memcpy(metadata, "linear\0\0", 8u);
        raw_size = cicp_zlib(metadata + 8, icc, icc_size);
        offset += cicp_chunk(png + offset, "iCCP", metadata, raw_size + 8u);
    }
    if (use_lower) {
        cicp_be32(metadata, 50000u);
        offset += cicp_chunk(png + offset, "gAMA", metadata, 4u);
        for (i = 0u; i < 8u; ++i) {
            cicp_be32(metadata + i * 4u, (unsigned int)(xy[i] * 100000 + 0.5));
        }
        offset += cicp_chunk(png + offset, "cHRM", metadata, 32u);
        if (specimen == 12) {
            metadata[0] = 0u;
            offset += cicp_chunk(png + offset, "sRGB", metadata, 1u);
        }
    }
    for (i = 0u; i < 4u; ++i) {
        for (c = 0u; c < 3u; ++c) {
            sample = source[i][gray ? 0u : c];
            input[i][c] = depth == 16 ? sample / 65535.0 :
                (sample >> 8) / 255.0;
            palette[i * 3u + c] = (unsigned char)(sample >> 8);
        }
    }
    if (indexed) {
        offset += cicp_chunk(png + offset, "PLTE", palette, sizeof(palette));
    }
    if (specimen == 32 || specimen == 33) {
        for (i = 0u; i < 4u; ++i) {
            metadata[i] = (unsigned char)(source[i][3] >> 8);
        }
        offset += cicp_chunk(png + offset, "tRNS", metadata, 4u);
    }
    if (specimen == 23) {
        offset += cicp_chunk(png + offset, "cICP", codes, 4u);
    }
    if (specimen == 9) {
        for (c = 0u; c < 3u; ++c) {
            background[c * 2u] = 0u;
            background[c * 2u + 1u] = (unsigned char)(51u * (c + 1u));
        }
        offset += cicp_chunk(png + offset, "bKGD", background, 6u);
    }
    if (apng) {
        cicp_be32(animation, 2u);
        cicp_be32(animation + 4, 1u);
        offset += cicp_chunk(png + offset, "acTL", animation, 8u);
        memset(animation, 0, sizeof(animation));
        cicp_be32(animation + 4, 4u);
        cicp_be32(animation + 8, 1u);
        animation[21] = 1u;
        animation[23] = 10u;
        offset += cicp_chunk(png + offset, "fcTL", animation, 26u);
    }
    raw_size = 0u;
    for (pass = 0u; pass < (adam7 ? 7u : 1u); ++pass) {
        if (adam7 && (pass == 1u || pass == 2u || pass == 4u || pass == 6u)) {
            continue;
        }
        raw[raw_size++] = 0u;
        if (indexed) {
            raw[raw_size++] = 0x1bu;
            continue;
        }
        for (i = adam7 ? start[pass] : 0u; i < 4u;
             i += adam7 ? step[pass] : 1u) {
            for (c = 0u; c < (size_t)channels; ++c) {
                k = has_alpha && c == (size_t)channels - 1u ? 3u :
                    gray ? 0u : c;
                sample = source[i][k];
                raw[raw_size++] = (unsigned char)(sample >> 8);
                if (depth == 16) {
                    raw[raw_size++] = (unsigned char)sample;
                }
            }
        }
    }
    compressed_size = cicp_zlib(compressed, raw, raw_size);
    offset += cicp_chunk(png + offset, "IDAT", compressed, compressed_size);
    if (apng) {
        cicp_be32(animation, 1u);
        offset += cicp_chunk(png + offset, "fcTL", animation, 26u);
        cicp_be32(metadata, 2u);
        memcpy(metadata + 4, compressed, compressed_size);
        offset += cicp_chunk(png + offset, "fdAT", metadata,
                             compressed_size + 4u);
    }
    if (specimen == 24) {
        offset += cicp_chunk(png + offset, "cICP", codes, 4u);
    }
    offset += cicp_chunk(png + offset, "IEND", NULL, 0u);
    status = sixel_chunk_create_from_memory(&chunk, png, offset, NULL,
                                             allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = create_loader_component_by_name(native ? "libpng" : "builtin",
                                               allocator, (void **)&component);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (unmanaged) {
        engine = SIXEL_CMS_ENGINE_NONE;
    }
    status = sixel_loader_component_setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE, &engine);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
        SIXEL_LOADER_OPTION_REQUIRE_STATIC, apng ? &zero : &one);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
        SIXEL_LOADER_OPTION_USE_PALETTE, &one);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
        SIXEL_LOADER_OPTION_REQCOLORS, &colors);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_PREFER_FLOAT32, &zero);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (specimen == 10 || specimen == 30) {
        bg[0] = 51u;
        bg[1] = 102u;
        bg[2] = 153u;
        status = sixel_loader_component_setopt(component,
            SIXEL_LOADER_OPTION_BGCOLOR, bg);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }
    callback = (loader_probe_callback_state_t){ NULL, cicp_capture, &probe };
    status = sixel_loader_component_load(component, chunk,
                                           capture_frame_trampoline, &callback);
    if (SIXEL_FAILED(status) || probe.count != (apng ? 2 : 1)) {
        fprintf(stderr, "cICP %d native=%d load=%x callbacks=%d %s\n",
            specimen, native, status, probe.count,
            sixel_helper_get_additional_message());
        goto cleanup;
    }
    if (byte_expected) {
        if ((!probe.palette && probe.format != SIXEL_PIXELFORMAT_RGB888) ||
            probe.colorspace != SIXEL_COLORSPACE_GAMMA) {
            fprintf(stderr, "cICP byte format=%x space=%d\n",
                    probe.format, probe.colorspace);
            goto cleanup;
        }
    } else if (probe.format != (specimen == 26 ? SIXEL_PIXELFORMAT_RGBFLOAT32 :
                    SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) ||
               probe.colorspace != (specimen == 26 ? SIXEL_COLORSPACE_GAMMA :
                    SIXEL_COLORSPACE_LINEAR)) {
        fprintf(stderr, "cICP float format=%x space=%d\n",
                probe.format, probe.colorspace);
        goto cleanup;
    }
    /* The fallback specimen uses serialized s15Fixed16 colorants rather
     * than analytic primaries, so it has the ICC quantization budget. */
    tolerance = engine == SIXEL_CMS_ENGINE_BUILTIN && specimen != 16
        ? 2e-6 : 2e-4;
    for (i = 0u; i < 4u; ++i) {
        for (c = 0u; c < 3u; ++c) {
            lin[c] = cicp_decode(input[i][c]);
        }
        for (c = 0u; c < 3u; ++c) {
            value = 0.0;
            for (k = 0u; k < 3u; ++k) {
                value += matrix[c][k] * lin[k];
            }
            if (specimen == 16) {
                value = input[i][c];
            } else if (specimen == 17) {
                value = input[i][c] * input[i][c];
            } else if (byte_expected && !apng) {
                value = input[i][c];
            }
            expected[i][c] = value;
        }
    }
    for (i = 0u; i < 4u; ++i) {
        for (c = 0u; c < 3u; ++c) {
            value = expected[i][c];
            if (specimen == 9 || specimen == 10 || specimen == 30) {
                alpha = depth == 16 ? source[i][3] / 65535.0 :
                    (source[i][3] >> 8) / 255.0;
                lin[0] = cicp_decode(0.2);
                lin[1] = cicp_decode(0.4);
                lin[2] = cicp_decode(0.6);
                if (specimen == 9) {
                    value = value * alpha;
                    for (k = 0u; k < 3u; ++k) {
                        value += matrix[c][k] * lin[k] * (1.0 - alpha);
                    }
                } else {
                    value = value * alpha + lin[c] * (1.0 - alpha);
                }
            }
            if (specimen == 26) {
                value = cicp_encode(value);
            }
            if (byte_expected) {
                value = floor(fmin(1.0, fmax(0.0, value)) * 255.0 + 0.5);
                if (probe.bytes[i * 3u + c] != (unsigned char)value) {
                    fprintf(stderr, "cICP %d pixel=%zu channel=%zu %u != %g\n",
                        specimen, i, c, probe.bytes[i * 3u + c], value);
                    goto cleanup;
                }
            } else if (!isfinite(probe.values[i * 3u + c]) ||
                fabs(probe.values[i * 3u + c] - value) > tolerance) {
                fprintf(stderr, "cICP %d pixel=%zu channel=%zu %.9g != %.9g\n",
                    specimen, i, c, probe.values[i * 3u + c], value);
                goto cleanup;
            }
        }
    }
    if (has_alpha && specimen != 9 && specimen != 10 && specimen != 30 &&
        (!probe.alpha_zero || (!probe.palette &&
         memcmp(probe.mask, "\0\0\1\0", 4u)))) {
        fprintf(stderr, "cICP zero alpha coverage mismatch\n");
        goto cleanup;
    }
    if ((specimen == 15 || specimen == 33) && !probe.palette) {
        fprintf(stderr, "cICP identity lost byte palette fast path\n");
        goto cleanup;
    }
    result = 0;
cleanup:
    sixel_helper_set_loader_transparent_policy(policy);
    sixel_cms_set_engine(previous);
    sixel_loader_component_unref(component);
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_cdef_free(definition);
    if (allocator != NULL) {
        sixel_allocator_free(allocator, icc);
        sixel_allocator_unref(allocator);
    }
    return result;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
