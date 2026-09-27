/* SPDX-License-Identifier: MIT */
/* In-memory source integers and independent color equations. Profile export
 * constructs the input only; expected values never come from a CMS call. */
#include <math.h>
#include "tests/loader/pixelformat_test_common.h"
#include "tests/loader/builtin/loader_builtin_memory_test_common.h"
#include "tests/loader/bmp_precision_test_common.h"
#include "src/cms.h"
#include "src/loader-common.h"

/* D65 Display P3 to linear sRGB, derived from primary chromaticities. */
static double const bmpc_matrix[3][3] = {
    {1.2249401762805598, -0.2249401762805598, 0.0},
    {-0.0420569547096882, 1.0420569547096882, 0.0},
    {-0.0196375545903344, -0.0786360455506319, 1.0982736001409663}
};

/* Independent Decimal oracle: tests/data/color-numeric/cmyk.json, samples
 * 0, 1, 5, 7. These include a non-grid CMYK sample and nonzero black ink. */
static unsigned char const bmpc_ink[4][4] = {
    {0, 255, 255, 0}, {255, 0, 255, 0}, {64, 128, 192, 32}, {255, 0, 0, 255}
};
static double const bmpc_ink_rgb[4][3] = {
    {0.9999780822715364, 0.1000460786471453, 0.0999704226399156},
    {0.0999764466764070, 1.0000446104187030, 0.0999661094283052},
    {0.7615545180131809, 0.5357130583878900, 0.3097836642010818},
    {0.0000357867321950, 0.8999507719530315, 0.9000159193336047}
};

typedef struct bmpc_spec {
    int bpp;
    int compression;
    int profile;
    int calibrated;
    int alpha;
    int background;
    int keep;
    int forced;
    int byte_output;
    int narrow;
    int gamma_output;
    int topdown;
    int cmyk;
    int reject;
    int calls;
    int engine;
    unsigned int bits[4];
    unsigned int masks[4];
    unsigned int raw[4][4];
    double expected[4][3];
    unsigned char mask[4];
} bmpc_spec_t;

static size_t bmpc_fail_size;
static int bmpc_failed;

static void *
bmpc_malloc(size_t size)
{
    if (size == bmpc_fail_size && !bmpc_failed) {
        bmpc_failed = 1;
        return NULL;
    }
    return malloc(size);
}

static double
bmpc_decode(double v)
{
    return v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
}

static double
bmpc_encode(double v)
{
    return copysign(fabs(v) <= 0.0031308 ? 12.92 * fabs(v) :
        1.055 * pow(fabs(v), 1.0 / 2.4) - 0.055, v);
}

static void
bmpc_u32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static void
bmpc_config(bmpc_spec_t *p, int specimen)
{
    memset(p, 0, sizeof(*p));
    p->bpp = 24;
    p->profile = 1;
    p->keep = 1;
    p->bits[0] = p->bits[1] = p->bits[2] = p->bits[3] = 8u;
    switch (specimen) {
    case 2: case 3: case 4: case 5: case 34: case 35:
        p->alpha = 1;
        p->background = specimen >= 3 && specimen <= 5;
        p->keep = specimen != 3 && specimen != 4;
        p->gamma_output = specimen == 4;
        break;
    case 6: case 7:
        p->profile = 0;
        p->calibrated = specimen == 6 ? 1 : 2;
        break;
    case 8: p->bpp = 8; break;
    case 9: p->bpp = 8; p->compression = 1; break;
    case 10: p->bpp = 4; p->compression = 2; break;
    case 11: case 12: case 16: case 17:
        p->bpp = 32;
        p->bits[0] = p->bits[1] = p->bits[2] = 10u;
        p->profile = specimen == 12 || specimen == 17;
        p->topdown = specimen == 16;
        break;
    case 13:
        p->bits[0] = p->bits[1] = p->bits[2] = 7u;
        p->bits[3] = 11u;
        p->alpha = p->background = 1;
        p->keep = 0;
        break;
    case 14: case 15:
        p->bpp = 16;
        p->bits[0] = p->bits[2] = 5u;
        p->bits[1] = 6u;
        p->profile = specimen == 14;
        p->forced = specimen == 15;
        break;
    case 18: case 19: case 20: case 21:
        p->profile = 0;
        p->byte_output = specimen != 21;
        p->alpha = specimen != 18;
        p->background = specimen >= 20;
        p->keep = specimen != 21;
        break;
    case 22: case 23: case 24: case 25: case 31: case 33:
    case 42: case 43: case 45:
        p->bpp = specimen == 24 ? 8 : specimen == 25 ? 4 : 32;
        p->cmyk = 1;
        p->compression = specimen == 24 ? 12 : specimen == 25 ? 13 : 11;
        p->profile = specimen == 22 ? 0 : 1;
        p->byte_output = specimen == 22 || specimen == 31;
        break;
    case 26: p->gamma_output = 1; break;
    case 27: p->byte_output = p->narrow = 1; break;
    case 30: p->byte_output = 1; break;
    case 36: case 37: case 38: case 39:
        p->profile = 0;
        p->bpp = specimen == 38 ? 16 : 32;
        p->bits[0] = 30u;
        p->bits[1] = p->bits[2] = 1u;
        break;
    case 40: case 41:
        p->alpha = 1;
        break;
    default: break;
    }
    if (p->alpha) {
        p->bpp = 32;
    }
    if (!p->cmyk && (p->bpp == 16 || p->bpp == 32)) {
        p->compression = specimen == 41 ? 0 : 3;
    }
    p->reject = specimen == 32 || specimen == 33 ||
        (specimen >= 36 && specimen <= 38);
}

static void
bmpc_values(bmpc_spec_t *p, int specimen)
{
    unsigned int max;
    unsigned int shift;
    int i;
    int c;
    int k;
    int managed;
    double v[3];
    double alpha;
    double expected;

    shift = 0u;
    managed = (p->profile || p->calibrated) && specimen != 17 &&
        specimen != 30 && specimen != 31;
    for (c = 0; c < 4; ++c) {
        max = (1u << p->bits[c]) - 1u;
        if (c < 3 || p->alpha) {
            p->masks[c] = max << shift;
            shift += p->bits[c];
        }
        for (i = 0; i < 4; ++i) {
            p->raw[i][c] = c == 3 ? max * (unsigned int)i / 3u :
                i == c ? max : i == 3 ? max / 4u * (unsigned int)(c + 1) : 0u;
            if (c == 3 && specimen == 19) {
                p->raw[i][c] = i == 0 ? 0u : max;
            } else if (c == 3 && specimen == 20) {
                p->raw[i][c] = max;
            } else if (c == 3 && (specimen == 40 || specimen == 41)) {
                p->raw[i][c] = 0u;
            }
        }
    }
    if (specimen == 41) {
        /* BI_RGB uses conventional B,G,R,A order, not custom masks. */
        p->masks[0] = 0xff0000u;
        p->masks[1] = 0xff00u;
        p->masks[2] = 0xffu;
        p->masks[3] = 0xff000000u;
    }
    for (i = 0; i < 4; ++i) {
        for (c = 0; c < 3; ++c) {
            v[c] = (float)((double)p->raw[i][c] /
                (double)((1u << p->bits[c]) - 1u));
            if (p->calibrated) {
                v[c] = pow(v[c], p->calibrated == 1 ? 2.0 :
                    c == 0 ? 1.0 : c == 1 ? 2.0 : 4.0);
            } else {
                v[c] = bmpc_decode(v[c]);
            }
        }
        alpha = !p->alpha || specimen == 41 ? 1.0 :
            (float)((double)p->raw[i][3] /
                (double)((1u << p->bits[3]) - 1u));
        p->mask[i] = p->alpha && alpha == 0.0 &&
            (!p->background || p->keep);
        for (c = 0; c < 3; ++c) {
            expected = v[c];
            if (managed && !p->cmyk) {
                expected = 0.0;
                for (k = 0; k < 3; ++k) {
                    expected += bmpc_matrix[c][k] * v[k];
                }
            } else if (p->cmyk) {
                expected = managed ? bmpc_ink_rgb[i][c] :
                    floor((255.0 - bmpc_ink[i][c]) *
                        (255.0 - bmpc_ink[i][3]) / 255.0 + 0.5) / 255.0;
            }
            if (p->background && !p->mask[i]) {
                expected = expected * alpha +
                    bmpc_decode((c + 1) * 0.2) * (1.0 - alpha);
            }
            if (p->gamma_output || (specimen == 27)) {
                expected = bmpc_encode(expected);
            }
            if (p->byte_output) {
                if (!p->cmyk && specimen != 27) {
                    expected = (double)p->raw[i][c] / 255.0;
                }
                expected = floor(fmin(1.0, fmax(0.0, expected)) * 255.0 + 0.5);
            }
            p->expected[i][c] = expected;
        }
    }
}

static SIXELSTATUS
bmpc_capture(sixel_frame_t *frame, void *context)
{
    bmpc_spec_t *p;
    unsigned char const *bytes;
    float const *pixels;
    int format;
    int i;
    int c;
    int has_mask;
    double actual;
    double bound;

    p = context;
    ++p->calls;
    format = p->byte_output ? SIXEL_PIXELFORMAT_RGB888 : p->gamma_output ?
        SIXEL_PIXELFORMAT_RGBFLOAT32 : SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    if (frame->width != 2 || frame->height != 2 ||
        frame->pixelformat != format ||
        frame->colorspace != (p->byte_output || p->gamma_output ?
            SIXEL_COLORSPACE_GAMMA : SIXEL_COLORSPACE_LINEAR)) {
        fprintf(stderr, "BMP precision format=%x expected=%x space=%d\n",
                frame->pixelformat, format, frame->colorspace);
        return SIXEL_BAD_INPUT;
    }
    bytes = sixel_frame_get_pixels(frame);
    pixels = (float const *)bytes;
    /* ICC s15Fixed16 colorants have a propagated matrix budget. Calibrated
     * endpoints use 2.30 and need only float rounding. CMYK uses the fixed
     * Decimal oracle and its native-CMM XYZ u1.15 interpolation budget. */
    bound = p->profile ? 3e-4 : 1e-6;
    if (p->cmyk && p->profile && !p->byte_output) {
        bound = p->engine == SIXEL_CMS_ENGINE_BUILTIN ? 4e-6 : 3.3e-4;
    }
    if (p->byte_output) {
        bound = p->narrow ? 1.0 : 0.0;
    }
    has_mask = 0;
    for (i = 0; i < 4; ++i) {
        has_mask |= p->mask[i];
        for (c = 0; c < 3; ++c) {
            actual = p->byte_output ? bytes[i * 3 + c] : pixels[i * 3 + c];
            if (!isfinite(actual) || fabs(actual - p->expected[i][c]) > bound) {
                fprintf(stderr, "BMP p=%d c=%d %.12g != %.12g bound=%g\n",
                        i, c, actual, p->expected[i][c], bound);
                return SIXEL_BAD_INPUT;
            }
        }
    }
    if (has_mask && (!frame->alpha_zero_is_transparent ||
        frame->transparent_mask_size != 4u ||
        memcmp(frame->transparent_mask, p->mask, 4u) != 0)) {
        fprintf(stderr, "BMP zero coverage mask mismatch\n");
        return SIXEL_BAD_INPUT;
    }
    if (!has_mask && frame->transparent_mask != NULL) {
        return SIXEL_BAD_INPUT;
    }
    return SIXEL_OK;
}

static size_t
bmpc_fixture(unsigned char *bytes, bmpc_spec_t *p,
               unsigned char const *icc, size_t icc_size, int specimen)
{
    /* D65 P3 primary XYZ columns, encoded as signed 2.30 endpoints. */
    static double const xyz[9] = {
        .4865709486482162, .2289745640697488, 0.0,
        .2656676931690931, .6917385218365064, .0451133818589026,
        .1982172852343625, .0792869140937450, 1.0439443689009759
    };
    size_t offset;
    size_t size;
    size_t stride;
    unsigned int value;
    unsigned int shift;
    int row;
    int x;
    int i;
    int c;

    memset(bytes, 0, 8192u);
    memcpy(bytes, "BM", 2u);
    bmpc_u32(bytes + 14, 124u);
    bmpc_u32(bytes + 18, 2u);
    bmpc_u32(bytes + 22, p->topdown ? 0xfffffffeu : 2u);
    bytes[26] = 1;
    bytes[28] = (unsigned char)p->bpp;
    bmpc_u32(bytes + 30, (unsigned int)p->compression);
    bmpc_u32(bytes + 70, icc_size != 0u ? 0x4d424544u :
        p->calibrated ? 0u : 0x73524742u);
    if (specimen == 44 || specimen == 45) {
        memcpy(bytes + 70, "MBED", 4u);
    }
    for (c = 0; c < 4; ++c) {
        bmpc_u32(bytes + 54 + c * 4, p->masks[c]);
    }
    if (p->calibrated) {
        for (i = 0; i < 9; ++i) {
            bmpc_u32(bytes + 74 + i * 4,
                (unsigned int)floor(xyz[i] * 1073741824.0 + 0.5));
        }
        bmpc_u32(bytes + 110, p->calibrated == 1 ? 32768u : 65536u);
        bmpc_u32(bytes + 114, 32768u);
        bmpc_u32(bytes + 118, p->calibrated == 1 ? 32768u : 16384u);
    }
    if (specimen == 36) {
        bmpc_u32(bytes + 54, 0x3ffffffdu);
    } else if (specimen == 37) {
        bmpc_u32(bytes + 58, 1u);
    } else if (specimen == 38) {
        bmpc_u32(bytes + 54, 0x1f000u);
    }
    offset = 138u;
    if (p->bpp < 16) {
        bmpc_u32(bytes + 46, 4u);
        for (i = 0; i < 4; ++i) {
            for (c = 0; c < 4; ++c) {
                bytes[offset++] = p->cmyk ? bmpc_ink[i][c] :
                    c == 3 ? 0u : (unsigned char)p->raw[i][2 - c];
            }
        }
    }
    bmpc_u32(bytes + 10, (unsigned int)offset);
    size = offset;
    stride = (size_t)((2 * p->bpp + 31) / 32) * 4u;
    for (row = 0; row < 2; ++row) {
        i = (p->topdown ? row : 1 - row) * 2;
        if (p->bpp < 16 && p->compression != 0) {
            if (p->bpp == 4) {
                bytes[size++] = 2u;
                bytes[size++] = (unsigned char)((i << 4) | (i + 1));
            } else {
                bytes[size++] = 1u;
                bytes[size++] = (unsigned char)i;
                bytes[size++] = 1u;
                bytes[size++] = (unsigned char)(i + 1);
            }
            bytes[size++] = 0u;
            bytes[size++] = row == 1 ? 1u : 0u;
            continue;
        }
        for (x = 0; x < 2; ++x, ++i) {
            if (p->cmyk) {
                memcpy(bytes + size + x * 4, bmpc_ink[i], 4u);
            } else if (p->bpp == 24) {
                for (c = 0; c < 3; ++c) {
                    bytes[size + x * 3 + c] = (unsigned char)p->raw[i][2 - c];
                }
            } else if (p->bpp == 8) {
                bytes[size + x] = (unsigned char)i;
            } else {
                value = 0u;
                for (c = 0; c < (p->alpha ? 4 : 3); ++c) {
                    shift = 0u;
                    while (((p->masks[c] >> shift) & 1u) == 0u) {
                        ++shift;
                    }
                    value |= p->raw[i][c] << shift;
                }
                bmpc_u32(bytes + size + x * (p->bpp / 8), value);
            }
        }
        size += stride;
    }
    bmpc_u32(bytes + 34, (unsigned int)(size - offset));
    if (icc_size != 0u) {
        bmpc_u32(bytes + 126, (unsigned int)(size - 14u));
        bmpc_u32(bytes + 130, (unsigned int)icc_size);
        memcpy(bytes + size, icc, icc_size);
        size += icc_size;
    }
    bmpc_u32(bytes + 2, (unsigned int)size);
    return size;
}

int
bmp_precision_test(int specimen, int engine)
{
    unsigned char bytes[8192];
    unsigned char ink_profile[4096];
    unsigned char bg[3] = {51u, 102u, 153u};
    unsigned char *icc;
    size_t icc_size;
    size_t size;
    size_t i;
    int previous_engine;
    int previous_policy;
    int value;
    int result;
    int available;
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_color_def_t *definition;
    sixel_chunk_t *chunk;
    sixel_loader_component_t *component;
    loader_probe_callback_state_t callback;
    bmpc_spec_t probe;

    previous_engine = sixel_cms_get_engine();
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    available = (int)sixel_cms_get_engine() == engine;
    sixel_cms_set_engine((sixel_cms_engine_t)previous_engine);
    if (!available) {
        return 77;
    }
    allocator = NULL;
    definition = NULL;
    component = NULL;
    chunk = NULL;
    icc = NULL;
    icc_size = 0u;
    result = 1;
    bmpc_fail_size = 0u;
    bmpc_failed = 0;
    previous_policy = loader_transparent_policy();
    bmpc_config(&probe, specimen);
    probe.engine = engine;
    bmpc_values(&probe, specimen);
    sixel_helper_set_loader_transparent_policy(probe.keep ?
        SIXEL_LOADER_ALPHA_POLICY_KEEP : SIXEL_LOADER_ALPHA_POLICY_COMPOSITE);
    status = sixel_allocator_new(&allocator, bmpc_malloc, calloc,
                                  realloc, free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (probe.profile) {
        if (probe.cmyk && specimen != 31) {
            if (edge_read_fixture("/tests/data/color-numeric/cmyk.icc",
                    ink_profile, sizeof(ink_profile), &icc_size) != 0) {
                goto end;
            }
            icc = sixel_allocator_malloc(allocator, icc_size);
            if (icc == NULL) {
                goto end;
            }
            memcpy(icc, ink_profile, icc_size);
        } else {
            definition = sixel_cdef_name("display-p3");
            if (definition == NULL || SIXEL_FAILED(sixel_cdef_export_icc(
                    definition, &icc, &icc_size, allocator))) {
                goto end;
            }
        }
        if (specimen == 30) {
            memcpy(icc + 16, "GRAY", 4u);
        } else if (specimen == 32) {
            memset(icc + 128, 0xff, 4u);
        } else if (specimen == 33) {
            for (i = 132u; i + 12u <= icc_size; i += 12u) {
                if (memcmp(icc + i, "A2B1", 4u) == 0) {
                    memcpy(icc + i, "A2B2", 4u);
                    break;
                }
            }
        }
    }
    size = bmpc_fixture(bytes, &probe, icc, icc_size, specimen);
    status = sixel_chunk_create_from_memory(&chunk, bytes, size, NULL,
                                             allocator);
    if (SIXEL_FAILED(status) || SIXEL_FAILED(create_loader_component_by_name(
            "builtin", allocator, (void **)&component))) {
        goto end;
    }
    value = specimen == 17 ? SIXEL_CMS_ENGINE_NONE : engine;
    status = sixel_loader_component_setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE, &value);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    value = probe.forced;
    status = sixel_loader_component_setopt(component,
        SIXEL_LOADER_COMPONENT_OPTION_PREFER_FLOAT32, &value);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (probe.background && SIXEL_FAILED(sixel_loader_component_setopt(
            component, SIXEL_LOADER_OPTION_BGCOLOR, bg))) {
        goto end;
    }
    if (specimen == 34 || specimen == 35) {
        bmpc_fail_size = specimen == 34 ? 48u : 4u;
    }
    callback = (loader_probe_callback_state_t){NULL, bmpc_capture, &probe};
    status = sixel_loader_component_load(component, chunk,
                                           capture_frame_trampoline, &callback);
    if (specimen == 34 || specimen == 35) {
        if (!SIXEL_FAILED(status) || probe.calls != 0 || !bmpc_failed) {
            goto end;
        }
        bmpc_fail_size = 0u;
        status = sixel_loader_component_load(component, chunk,
                                               capture_frame_trampoline,
                                               &callback);
    }
    result = probe.reject ?
        !(SIXEL_FAILED(status) && probe.calls == 0) :
        !(status == SIXEL_OK && probe.calls == 1);
    if (result != 0) {
        fprintf(stderr, "BMP specimen=%d status=%x calls=%d: %s\n",
                specimen, status, probe.calls,
                sixel_helper_get_additional_message());
    }
end:
    bmpc_fail_size = 0u;
    sixel_helper_set_loader_transparent_policy(previous_policy);
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
