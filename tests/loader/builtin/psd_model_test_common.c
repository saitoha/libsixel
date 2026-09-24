/* SPDX-License-Identifier: MIT */
/* Native Gray/Lab samples with independent scalar/D50 matrix expectations. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "src/cms.h"
#include "loader_builtin_memory_test_common.h"
#include "psd_model_test_common.h"
#include "tests/data/color-numeric/gray-samples.h"
#include "tests/data/color-numeric/profile-format.h"

#define PSDM_ICC 1u
#define PSDM_DISABLED 2u
#define PSDM_MASK 4u
#define PSDM_ALTERNATE 8u
#define PSDM_BAD_ICC 16u
#define PSDM_WRONG_ICC 32u
#define PSDM_NAN 64u
#define PSDM_ALPHA 128u
#define PSDM_NO_SPOT 256u
#define PSDM_BAD_TRC 512u
#define PSDM_NEUTRAL 1024u
#define PSDM_ENDPOINTS 2048u
#define PSDM_TWO 4096u
#define PSDM_SHORT 8192u
#define PSDM_SPOT_NAN 16384u
#define PSDM_CONSTANT 32768u

static void
psdm_u32(edge_writer_t *w, uint32_t value)
{
    edge_put_u16be(w, value >> 16);
    edge_put_u16be(w, value & 65535u);
}

static void
psdm_resource(edge_writer_t *w, unsigned int id,
                 unsigned char const *bytes, size_t size)
{
    edge_put_bytes(w, (unsigned char const *)"8BIM", 4u);
    edge_put_u16be(w, id);
    edge_put_u16be(w, 0u);
    psdm_u32(w, (uint32_t)size);
    edge_put_bytes(w, bytes, size);
    if (size & 1u) {
        edge_put_u8(w, 0u);
    }
}

static double
psdm_linear(double value)
{
    double v;

    v = fabs(value);
    v = v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
    return value < 0.0 ? -v : v;
}

static void
psdm_lab(double l, double a, double b, double rgb[3])
{
    /* Inverse sRGB primaries times Bradford D50->D65, derived with the
     * rational oracle in data/psd-tools/generate_duotone_reference.py. */
    static double const matrix[3][3] = {
        {3.134186364236819, -1.6172089589982748, -0.49069406400638393},
        {-0.9787485041906944, 1.9161300967735873, 0.03343339915999554},
        {0.07196392780224674, -0.22899387345320327, 1.4057537328964445}
    };
    static double const white[3] = {0.9642, 1.0, 0.8249};
    double xyz[3];
    unsigned int i;
    unsigned int j;

    xyz[1] = (l + 16.0) / 116.0;
    xyz[0] = xyz[1] + a / 500.0;
    xyz[2] = xyz[1] - b / 200.0;
    for (i = 0u; i < 3u; ++i) {
        xyz[i] = white[i] * (xyz[i] > 6.0 / 29.0 ? pow(xyz[i], 3.0) :
            (xyz[i] - 4.0 / 29.0) * 108.0 / 841.0);
    }
    for (i = 0u; i < 3u; ++i) {
        rgb[i] = 0.0;
        for (j = 0u; j < 3u; ++j) {
            rgb[i] += matrix[i][j] * xyz[j];
        }
    }
}

int
psd_model_test(int specimen)
{
    static struct {
        unsigned int mode, depth, version, compression, flags;
    } const cases[] = {
        {1, 8, 1, 0, 0},
        {1, 16, 1, 0, 0},
        {1, 32, 1, 0, 0},
        {1, 8, 1, 0, PSDM_ICC},
        {1, 16, 1, 0, PSDM_ICC},
        {1, 32, 1, 0, PSDM_ICC},
        {1, 16, 1, 0, PSDM_ICC | PSDM_DISABLED},
        {1, 32, 1, 0, PSDM_MASK},
        {1, 8, 1, 0, PSDM_ALTERNATE},
        {1, 8, 1, 0, PSDM_ICC | PSDM_WRONG_ICC},
        {1, 8, 1, 0, PSDM_ICC | PSDM_BAD_ICC},
        {1, 8, 1, 0, PSDM_ICC | PSDM_BAD_TRC},
        {1, 32, 1, 0, PSDM_NAN},
        {1, 8, 1, 0, PSDM_ALPHA},
        {1, 8, 1, 1, 0},
        {1, 16, 1, 2, 0},
        {1, 8, 2, 3, 0},
        {1, 8, 1, 0, PSDM_NO_SPOT},
        {9, 8, 1, 0, 0},
        {9, 16, 1, 0, 0},
        {9, 8, 1, 0, PSDM_ICC},
        {9, 16, 1, 0, PSDM_ICC},
        {9, 16, 1, 0, PSDM_ICC | PSDM_DISABLED},
        {9, 16, 1, 0, PSDM_MASK},
        {9, 8, 1, 0, PSDM_ALTERNATE},
        {9, 8, 1, 0, PSDM_ICC | PSDM_WRONG_ICC},
        {9, 8, 1, 0, PSDM_ICC | PSDM_BAD_ICC},
        {9, 16, 1, 0, PSDM_NEUTRAL},
        {9, 16, 1, 0, PSDM_ENDPOINTS},
        {9, 32, 1, 0, 0},
        {9, 8, 1, 0, PSDM_ALPHA},
        {9, 8, 1, 1, 0},
        {9, 16, 1, 2, 0},
        {9, 16, 2, 3, 0},
        {9, 16, 1, 0, PSDM_TWO},
        {1, 16, 1, 0, PSDM_TWO},
        {9, 16, 1, 0, PSDM_SHORT},
        {1, 32, 1, 0, PSDM_SPOT_NAN},
        {9, 16, 1, 0, PSDM_ICC | PSDM_CONSTANT}
    };
    static unsigned int const gray8[8] = {0, 1, 3, 32, 64, 128, 192, 255};
    static unsigned int const gray16[8] = {
        0, 1, 32767, 32896, 32897, 51201, 65534, 65535
    };
    static float const gray32[8] = {
        0.0f, 0.00390631f, 0.12501f, 0.50001f, 0.75001f, 0.99999f, 1.0f, 0.25f
    };
    static unsigned int const lab8[3][8] = {
        {0, 20, 64, 128, 192, 255, 80, 128},
        {128, 128, 24, 200, 150, 128, 255, 0},
        {128, 128, 200, 40, 150, 128, 0, 255}
    };
    /* Pixel 1 is the red pixel's native Lab16 triplet in psd-tools'
     * Photoshop colormodes/4x4_16bit_lab.psd. Keep the words, not an
     * upscaled Lab8 surrogate: opponent coordinates are fixed-point. */
    static unsigned int const lab16[3][8] = {
        {0, 35579, 16448, 32897, 49344, 65535, 20560, 32896},
        {32768, 53451, 6144, 51201, 38400, 32768, 65280, 0},
        {32768, 50657, 51200, 10241, 38400, 32768, 0, 65280}
    };
    static unsigned int const plates[8] = {255, 255, 240, 128, 64, 0, 250, 255};
    unsigned char file[8192];
    unsigned char resources[4096];
    unsigned char record[4096];
    unsigned char raw[256];
    edge_writer_t fw, rw, dw, pw;
    edge_loader_options_t options;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    double source[3][8];
    double coverage[2][8];
    double expected[3];
    double sample;
    double ink;
    double color;
    double solidity;
    float actual[24];
    float value;
    uint32_t bits, s1, s2;
    unsigned int mode, depth, version, compression, flags;
    unsigned int channels, extras, spot, k, i, j, code, previous;
    size_t n, row, length;
    int result;
    int reject;

    if (specimen < 159 || specimen > 197) {
        return 1;
    }
    mode = cases[specimen - 159].mode;
    depth = cases[specimen - 159].depth;
    version = cases[specimen - 159].version;
    compression = cases[specimen - 159].compression;
    flags = cases[specimen - 159].flags;
    channels = mode == 1u ? 1u : 3u;
    spot = (flags & (PSDM_MASK | PSDM_ALPHA)) ? 1u : 0u;
    extras = flags & PSDM_NO_SPOT ? 0u :
        spot || (flags & PSDM_TWO) ? 2u : 1u;
    reject = (flags & (PSDM_WRONG_ICC | PSDM_BAD_ICC | PSDM_BAD_TRC |
        PSDM_NAN | PSDM_ALPHA | PSDM_SHORT | PSDM_SPOT_NAN)) != 0u ||
        (mode == 9u && depth == 32u);
    fw = (edge_writer_t){file, sizeof(file), 0u, 0};
    rw = (edge_writer_t){resources, sizeof(resources), 0u, 0};
    dw = (edge_writer_t){record, sizeof(record), 0u, 0};
    pw = (edge_writer_t){raw, sizeof(raw), 0u, 0};
    if (extras) {
        psdm_u32(&dw, 1u);
        for (k = 0u; k < extras; ++k) {
            edge_put_u16be(&dw, flags & PSDM_ALTERNATE ? 0x2331u : 0u);
            for (j = 0u; j < 4u; ++j) {
                edge_put_u16be(&dw, (flags & PSDM_TWO) && k == 1u ?
                    (j != 1u && j < 3u ? 65535u : 0u) :
                    j == 1u ? 65535u : 0u);
            }
            edge_put_u16be(&dw, k == 1u && !spot ? 25u : 50u);
            edge_put_u8(&dw, k == spot || (flags & PSDM_TWO) ? 2u : 1u);
        }
        psdm_resource(&rw, 1077u, record, dw.length);
        dw.length = 0u;
        for (k = 0u; k < extras; ++k) {
            psdm_u32(&dw, k == 0u && (flags & PSDM_ALPHA) ? 0u : k + 10u);
        }
        psdm_resource(&rw, 1053u, record, dw.length);
    }
    if (flags & PSDM_ALTERNATE) {
        dw.length = 0u;
        edge_put_u16be(&dw, 1u);
        edge_put_u16be(&dw, 1u);
        psdm_u32(&dw, 10u);
        edge_put_u16be(&dw, 0u);
        for (j = 0u; j < 4u; ++j) {
            edge_put_u16be(&dw, j == 2u ? 65535u : 0u);
        }
        psdm_resource(&rw, 1067u, record, dw.length);
    }
    if (flags & PSDM_ICC) {
        if (mode == 1u && (flags & PSDM_WRONG_ICC)) {
            length = sizeof(pf_icc_0);
            memcpy(record, pf_icc_0, length);
        } else if (mode == 1u || (flags & PSDM_WRONG_ICC)) {
            length = sizeof(color_gray_icc);
            memcpy(record, color_gray_icc, length);
        } else {
            length = sizeof(pf_icc_6);
            memcpy(record, pf_icc_6, length);
        }
        if (flags & PSDM_BAD_TRC) {
            memcpy(record + 180u, "junk", 4u);
        }
        if (flags & PSDM_CONSTANT) {
            /* The fixed mft1 profile has 3 identity input tables, a 2^3
             * CLUT and 3 identity output tables. A constant neutral CLUT
             * proves that source ICC is actually applied, not ignored. */
            memset(record + 1980u + 48u + 768u, 128, 24u);
        }
        psdm_resource(&rw, 1039u, record,
                       flags & PSDM_BAD_ICC ? 8u : length);
    }
    edge_put_bytes(&fw, (unsigned char const *)"8BPS", 4u);
    edge_put_u16be(&fw, version);
    for (i = 0u; i < 6u; ++i) {
        edge_put_u8(&fw, 0u);
    }
    edge_put_u16be(&fw, channels + extras);
    psdm_u32(&fw, 1u);
    psdm_u32(&fw, 8u);
    edge_put_u16be(&fw, depth);
    edge_put_u16be(&fw, mode);
    psdm_u32(&fw, 0u);
    psdm_u32(&fw, (uint32_t)rw.length);
    edge_put_bytes(&fw, resources, rw.length);
    psdm_u32(&fw, 0u);
    if (version == 2u) {
        psdm_u32(&fw, 0u);
    }
    edge_put_u16be(&fw, compression);
    for (k = 0u; k < channels + extras; ++k) {
        for (i = 0u; i < 8u; ++i) {
            if (k < channels) {
                code = mode == 1u ? (depth == 8u ? gray8[i] : gray16[i]) :
                    depth == 8u ? lab8[k][i] : lab16[k][i];
                if (k && (flags & PSDM_NEUTRAL)) {
                    code = 32768u;
                } else if (k && (flags & PSDM_ENDPOINTS)) {
                    code = (i + k) % 2u ? 0u : 65535u;
                }
                sample = depth == 32u ? gray32[i] :
                    code / (depth == 8u ? 255.0 : 65535.0);
                if (specimen == 161 && i >= 6u) {
                    sample = i == 6u ? 1.25 : -0.125;
                }
                source[k][i] = sample;
            } else {
                sample = k == channels + spot || (flags & PSDM_TWO) ?
                    plates[(i + (k > channels + spot ? 3u : 0u)) % 8u] /
                    255.0 : 0.0;
                if (k >= channels + spot) {
                    coverage[k - channels - spot][i] = sample;
                }
            }
            if (depth == 8u) {
                edge_put_u8(&pw, (unsigned int)(sample * 255.0 + 0.5));
            } else if (depth == 16u) {
                edge_put_u16be(&pw, (unsigned int)(sample * 65535.0 + 0.5));
            } else {
                value = (float)sample;
                memcpy(&bits, &value, sizeof(bits));
                if ((k == 0u && (flags & PSDM_NAN)) ||
                    (k == channels && (flags & PSDM_MASK)) ||
                    (k >= channels && (flags & PSDM_SPOT_NAN))) {
                    bits = UINT32_C(0x7fc00000);
                }
                psdm_u32(&pw, bits);
            }
        }
    }
    row = 8u * (depth / 8u);
    if (compression == 1u) {
        for (k = 0u; k < channels + extras; ++k) {
            edge_put_u16be(&fw, (unsigned int)row + 1u);
        }
        for (k = 0u; k < channels + extras; ++k) {
            edge_put_u8(&fw, (unsigned int)row - 1u);
            edge_put_bytes(&fw, raw + k * row, row);
        }
    } else if (compression >= 2u) {
        if (compression == 3u) {
            for (k = 0u; k < channels + extras; ++k) {
                for (i = 7u; i > 0u; --i) {
                    n = k * row + i * (depth / 8u);
                    if (depth == 8u) {
                        raw[n] -= raw[n - 1u];
                    } else {
                        code = raw[n] * 256u + raw[n + 1u];
                        previous = raw[n - 2u] * 256u + raw[n - 1u];
                        code = (code - previous) & 65535u;
                        raw[n] = (unsigned char)(code >> 8);
                        raw[n + 1u] = (unsigned char)code;
                    }
                }
            }
        }
        edge_put_u16be(&fw, 0x7801u);
        edge_put_u8(&fw, 1u);
        edge_put_u16le(&fw, (unsigned int)pw.length);
        edge_put_u16le(&fw, 65535u - (unsigned int)pw.length);
        edge_put_bytes(&fw, raw, pw.length);
        s1 = 1u;
        s2 = 0u;
        for (n = 0u; n < pw.length; ++n) {
            s1 = (s1 + raw[n]) % 65521u;
            s2 = (s2 + s1) % 65521u;
        }
        psdm_u32(&fw, s2 * 65536u + s1);
    } else {
        edge_put_bytes(&fw, raw, pw.length);
    }
    if (fw.failed || rw.failed || dw.failed || pw.failed) {
        return 1;
    }
    edge_loader_options_init(&options);
    options.require_static = 1;
    if ((flags & PSDM_ICC) && !(flags & PSDM_DISABLED)) {
        options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;
    }
    memset(&probe, 0, sizeof(probe));
    result = edge_load_buffer_options("PSD Gray/Lab spots", file,
        fw.length - ((flags & PSDM_SHORT) ? 1u : 0u),
        &options, &probe, &status);
    if (reject) {
        return result != 0 || SIXEL_SUCCEEDED(status) || probe.callback_count;
    }
    if (result != 0 || SIXEL_FAILED(status) || probe.callback_count != 1 ||
        probe.width[0] != 8 || probe.height[0] != 1 ||
        probe.mask_size[0] != 0u) {
        fprintf(stderr, "PSD model=%d status=%x calls=%d\n", specimen,
                status, probe.callback_count);
        return 1;
    }
    if (flags & PSDM_NO_SPOT) {
        if (probe.pixelformat[0] != SIXEL_PIXELFORMAT_RGB888 ||
            probe.colorspace[0] != SIXEL_COLORSPACE_GAMMA ||
            probe.rgb_size[0] != 24u) {
            return 1;
        }
        for (i = 0u; i < 24u; ++i) {
            if (probe.rgb[0][i] != gray8[i / 3u]) {
                return 1;
            }
        }
        return 0;
    }
    if (probe.pixelformat[0] != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        probe.colorspace[0] != SIXEL_COLORSPACE_LINEAR ||
        probe.rgb_size[0] != sizeof(actual)) {
        return 1;
    }
    memcpy(actual, probe.rgb[0], sizeof(actual));
    for (i = 0u; i < 8u; ++i) {
        if (mode == 9u) {
            sample = depth == 8u ? 255.0 : 65535.0 / 256.0;
            psdm_lab(source[0][i] * 100.0,
                source[1][i] * sample - 128.0,
                source[2][i] * sample - 128.0, expected);
            if (flags & PSDM_CONSTANT) {
                psdm_lab(12800.0 / 255.0, 0.0, 0.0, expected);
            }
        } else {
            sample = source[0][i];
            expected[0] = expected[1] = expected[2] =
                (flags & PSDM_ICC) && !(flags & PSDM_DISABLED) ?
                sample * sample : psdm_linear(sample);
        }
        for (k = 0u; k < ((flags & PSDM_TWO) ? 2u : 1u); ++k) {
            ink = 1.0 - coverage[k][i];
            solidity = k ? 0.25 : 0.5;
            for (j = 0u; j < 3u; ++j) {
                color = k ? (j != 1u ? 1.0 : 0.0) :
                    j == ((flags & PSDM_ALTERNATE) ? 2u : 1u) ? 1.0 : 0.0;
                expected[j] = (1.0 - ink) * expected[j] + ink *
                    (solidity * color + (1.0 - solidity) *
                     expected[j] * color);
            }
        }
        for (j = 0u; j < 3u; ++j) {
            if (!isfinite(actual[i * 3u + j]) ||
                fabs(actual[i * 3u + j] - expected[j]) > 4e-6) {
                fprintf(stderr, "PSD model=%d pixel=%u ch=%u %.9g != %.9g\n",
                    specimen, i, j, actual[i * 3u + j], expected[j]);
                return 1;
            }
        }
    }
    return 0;
}
