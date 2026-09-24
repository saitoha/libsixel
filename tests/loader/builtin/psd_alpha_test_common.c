/* SPDX-License-Identifier: MIT */
/* Native matte samples and independent premultiplied blend expectations.
 * No production CMS or compositor supplies the expected output values. */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "src/cms.h"
#include "loader_builtin_memory_test_common.h"
#include "psd_alpha_test_common.h"
#include "tests/data/color-numeric/minimal-profiles.h"

static void
psda_u32(edge_writer_t *w, uint32_t value)
{
    edge_put_u16be(w, value >> 16);
    edge_put_u16be(w, value & 65535u);
}

static void
psda_resource(edge_writer_t *w, unsigned int id,
                unsigned char const *data, size_t size)
{
    edge_put_bytes(w, (unsigned char const *)"8BIM", 4u);
    edge_put_u16be(w, id);
    edge_put_u16be(w, 0u);
    psda_u32(w, (uint32_t)size);
    edge_put_bytes(w, data, size);
    if (size & 1u) {
        edge_put_u8(w, 0u);
    }
}

static double
psda_linear(double value)
{
    double magnitude;

    magnitude = fabs(value);
    magnitude = magnitude <= 0.04045 ? magnitude / 12.92 :
        pow((magnitude + 0.055) / 1.055, 2.4);
    return value < 0.0 ? -magnitude : magnitude;
}

static double
psda_quantize(double value, unsigned int depth)
{
    double scale;

    if (depth == 32u) {
        return (float)value;
    }
    scale = depth == 16u ? 65535.0 : 255.0;
    return floor(value * scale + 0.5) / scale;
}

int
psd_alpha_test(int specimen)
{
    static unsigned int const alpha8[8] = {0, 255, 128, 64, 1, 160, 0, 128};
    static unsigned int const alpha16[8] = {
        0, 65535, 32769, 16641, 1, 41121, 0, 32897
    };
    static unsigned int const plate[8] = {0, 255, 128, 64, 255, 224, 255, 0};
    static double const p3[3] = {
        1.224939473660301, -0.04204725979332884, -0.019642226052377188
    };
    unsigned char file[4096];
    unsigned char resources[2048];
    unsigned char record[256];
    unsigned char layer[256];
    unsigned char raw[256];
    edge_writer_t fw;
    edge_writer_t rw;
    edge_writer_t dw;
    edge_writer_t lw;
    edge_writer_t pw;
    edge_loader_options_t options;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    double samples[7][8];
    double alpha[8];
    double expected[3];
    double background[3];
    double a;
    double ink;
    double color;
    double opacity;
    double next;
    double blend;
    float value;
    float actual[24];
    uint32_t bits;
    uint32_t s1;
    uint32_t s2;
    unsigned int depth;
    unsigned int version;
    unsigned int extras;
    unsigned int alpha_slot;
    unsigned int offset;
    unsigned int compression;
    unsigned int code;
    unsigned int i;
    unsigned int j;
    unsigned int k;
    size_t n;
    size_t row;
    int flag;
    int ids;
    int profiled;
    int bgcolor;
    int reject;
    int result;

    depth = specimen == 129 || specimen == 152 || specimen == 156 ? 16u :
        specimen == 130 || (specimen >= 144 && specimen <= 150) ||
        specimen == 154 ? 32u : 8u;
    version = specimen == 154 ? 2u : 1u;
    extras = specimen == 155 ? 4u : 3u;
    alpha_slot = specimen == 138 || specimen == 139 ? 1u : 0u;
    offset = (specimen >= 134 && specimen <= 136) || specimen == 141;
    flag = specimen != 138 && specimen != 141;
    ids = specimen != 134 && specimen != 137;
    profiled = (specimen >= 131 && specimen <= 133) || specimen == 156;
    bgcolor = specimen == 128 || specimen == 132 || specimen == 157;
    reject = (specimen >= 139 && specimen <= 143) ||
        (specimen >= 145 && specimen <= 147) || specimen == 149;
    compression = specimen == 151 ? 1u : specimen == 152 ? 2u :
        specimen == 153 ? 3u : 0u;
    fw = (edge_writer_t){file, sizeof(file), 0u, 0};
    rw = (edge_writer_t){resources, sizeof(resources), 0u, 0};
    dw = (edge_writer_t){record, sizeof(record), 0u, 0};
    lw = (edge_writer_t){layer, sizeof(layer), 0u, 0};
    pw = (edge_writer_t){raw, sizeof(raw), 0u, 0};
    psda_u32(&dw, 1u);
    for (k = offset; k < extras; ++k) {
        edge_put_u16be(&dw, specimen == 136 && k == 2u ? 0x2331u : 0u);
        for (j = 0u; j < 4u; ++j) {
            edge_put_u16be(&dw, k == 3u ?
                (j == 0u || j == 2u ? 65535u : 0u) :
                j == 1u ? 65535u : 0u);
        }
        edge_put_u16be(&dw, k == 3u ? 25u : 50u);
        edge_put_u8(&dw, k >= 2u ? 2u : 1u);
    }
    psda_resource(&rw, 1077u, record, dw.length);
    if (ids) {
        dw.length = 0u;
        for (k = specimen == 135 ? 1u : 0u; k < extras; ++k) {
            psda_u32(&dw, (specimen == 140 ? k == 2u : k == alpha_slot) ||
                (specimen == 143 && k == 1u) ? 0u : k + 10u);
        }
        psda_resource(&rw, 1053u, record,
                       dw.length - (specimen == 142 ? 4u : 0u));
    }
    if (specimen == 136) {
        dw.length = 0u;
        edge_put_u16be(&dw, 1u);
        edge_put_u16be(&dw, 1u);
        psda_u32(&dw, 12u);
        edge_put_u16be(&dw, 0u);
        for (j = 0u; j < 4u; ++j) {
            edge_put_u16be(&dw, j == 2u ? 65535u : 0u);
        }
        psda_resource(&rw, 1067u, record, dw.length);
    }
    if (profiled) {
        psda_resource(&rw, 1039u, color_minimal_display_p3,
                       sizeof(color_minimal_display_p3));
    }
    if (specimen == 137) {
        psda_u32(&lw, 48u);
        edge_put_u16be(&lw, 65535u);
        for (i = 0u; i < 18u; ++i) {
            edge_put_u8(&lw, 0u);
        }
        edge_put_bytes(&lw, (unsigned char const *)"8BIMnorm", 8u);
        psda_u32(&lw, UINT32_C(0xff000000));
        psda_u32(&lw, 12u);
        for (i = 0u; i < 16u; ++i) {
            edge_put_u8(&lw, 0u);
        }
    } else if (flag) {
        psda_u32(&lw, 0u);
        if (version == 2u) {
            psda_u32(&lw, 0u);
        }
        psda_u32(&lw, 0u);
        edge_put_bytes(&lw, (unsigned char const *)"8BIM", 4u);
        edge_put_bytes(&lw, (unsigned char const *)(depth == 8u ? "Mtrn" :
                        depth == 16u ? "Mt16" : "Mt32"), 4u);
        psda_u32(&lw, 0u);
        if (version == 2u) {
            psda_u32(&lw, 0u);
        }
    }
    edge_put_bytes(&fw, (unsigned char const *)"8BPS", 4u);
    edge_put_u16be(&fw, version);
    for (i = 0u; i < 6u; ++i) {
        edge_put_u8(&fw, 0u);
    }
    edge_put_u16be(&fw, 3u + extras);
    psda_u32(&fw, 1u);
    psda_u32(&fw, 8u);
    edge_put_u16be(&fw, depth);
    edge_put_u16be(&fw, 3u);
    psda_u32(&fw, 0u);
    psda_u32(&fw, (uint32_t)rw.length);
    edge_put_bytes(&fw, resources, rw.length);
    if (version == 2u) {
        psda_u32(&fw, 0u);
    }
    psda_u32(&fw, (uint32_t)lw.length);
    edge_put_bytes(&fw, layer, lw.length);
    edge_put_u16be(&fw, compression);
    for (i = 0u; i < 8u; ++i) {
        alpha[i] = psda_quantize(depth == 16u ? alpha16[i] / 65535.0 :
                                 alpha8[i] / 255.0, depth);
        if (specimen == 148) {
            bits = 1u;
            memcpy(&value, &bits, sizeof(value));
            alpha[i] = value;
        } else if (specimen == 149 || specimen == 150) {
            alpha[i] = 0.5;
        } else if (specimen == 157) {
            alpha[i] = 0.0;
        }
        for (j = 0u; j < 3u; ++j) {
            color = profiled ? (j == 0u ? 1.0 : 0.0) :
                j == 0u ? (i + 1u) / 16.0 : j == 1u ? (8u - i) / 16.0 : 0.75;
            if (specimen == 130) {
                color = j == 0u ? -0.25 : j == 1u ? 1.5 : 0.5;
            }
            samples[j][i] = psda_quantize(alpha[i] == 0.0 ? color :
                1.0 - alpha[i] + alpha[i] * color, depth);
            if (specimen == 148) {
                samples[j][i] = 1.0;
            } else if (specimen == 149 && i == 7u) {
                samples[j][i] = FLT_MAX;
            } else if (specimen == 150) {
                samples[j][i] = j == 0u ? 0.25 : j == 1u ? 1.25 : 0.5;
            }
        }
        for (k = 0u; k < extras; ++k) {
            samples[3u + k][i] = k == alpha_slot ? alpha[i] : k < 2u ?
                0.0 : psda_quantize(plate[(i + (k == 3u ? 3u : 0u)) % 8u] /
                                   255.0, depth);
            if ((specimen == 148 || specimen == 157) && k >= 2u) {
                samples[3u + k][i] = 1.0;
            }
        }
    }
    for (k = 0u; k < 3u + extras; ++k) {
        for (i = 0u; i < 8u; ++i) {
            if (depth != 32u) {
                code = (unsigned int)(samples[k][i] *
                       (depth == 16u ? 65535.0 : 255.0) + 0.5);
                if (depth == 8u) {
                    edge_put_u8(&pw, code);
                } else {
                    edge_put_u16be(&pw, code);
                }
            } else {
                value = (float)samples[k][i];
                memcpy(&bits, &value, sizeof(bits));
                if ((specimen == 144 && k == 4u) ||
                    (specimen == 145 && k == 3u)) {
                    bits = UINT32_C(0x7fc00000);
                } else if (specimen == 146 && k == 3u) {
                    bits = UINT32_C(0xbf000000);
                } else if (specimen == 147 && k == 3u) {
                    bits = UINT32_C(0x3fc00000);
                }
                psda_u32(&pw, bits);
            }
        }
    }
    row = 8u * (depth / 8u);
    if (compression == 1u) {
        for (k = 0u; k < 3u + extras; ++k) {
            edge_put_u16be(&fw, (unsigned int)row + 1u);
        }
        for (k = 0u; k < 3u + extras; ++k) {
            edge_put_u8(&fw, (unsigned int)row - 1u);
            edge_put_bytes(&fw, raw + k * row, row);
        }
    } else if (compression >= 2u) {
        if (compression == 3u) {
            for (k = 0u; k < 3u + extras; ++k) {
                for (n = row - 1u; n > 0u; --n) {
                    raw[k * row + n] -= raw[k * row + n - 1u];
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
        psda_u32(&fw, s2 * 65536u + s1);
    } else {
        edge_put_bytes(&fw, raw, pw.length);
    }
    if (fw.failed || rw.failed || dw.failed || lw.failed || pw.failed) {
        return 1;
    }
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.set_bgcolor = bgcolor;
    for (j = 0u; j < 3u; ++j) {
        options.bgcolor[j] = (unsigned char)(32u + 64u * j);
        background[j] = psda_linear(options.bgcolor[j] / 255.0);
    }
    if (profiled && specimen != 133) {
        options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;
    }
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    result = edge_load_buffer_options("PSD spot alpha", file, fw.length,
                                       &options, &probe, &status);
    if (reject) {
        return result != 0 || SIXEL_SUCCEEDED(status) || probe.callback_count;
    }
    if (result != 0 || SIXEL_FAILED(status) || probe.callback_count != 1 ||
        probe.width[0] != 8 || probe.height[0] != 1 ||
        probe.pixelformat[0] != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        probe.colorspace[0] != SIXEL_COLORSPACE_LINEAR ||
        probe.mask_size[0] != (bgcolor ? 0u : 8u) ||
        probe.rgb_size[0] != sizeof(actual)) {
        fprintf(stderr, "PSD alpha=%d status=%x callbacks=%d\n",
                specimen, status, probe.callback_count);
        return 1;
    }
    memcpy(actual, probe.rgb[0], sizeof(actual));
    for (i = 0u; i < 8u; ++i) {
        a = alpha[i];
        for (j = 0u; j < 3u; ++j) {
            color = a == 0.0 ? samples[j][i] : specimen == 148 ? 1.0 :
                (samples[j][i] - (1.0 - a)) / a;
            expected[j] = profiled ? (specimen == 133 ?
                (j == 0u ? 1.0 : 0.0) : p3[j]) : psda_linear(color);
            if (bgcolor) {
                expected[j] = expected[j] * a + background[j] * (1.0 - a);
            }
        }
        if (bgcolor) {
            a = 1.0;
        }
        for (k = 2u; k < extras; ++k) {
            ink = 1.0 - samples[3u + k][i];
            opacity = k == 3u ? 0.25 : 0.5;
            next = a + ink * (1.0 - a);
            if (ink == 0.0) {
                continue;
            }
            for (j = 0u; j < 3u; ++j) {
                color = k == 3u ? (j != 1u ? 1.0 : 0.0) :
                    j == (specimen == 136 ? 2u : 1u) ? 1.0 : 0.0;
                /* General source-over oracle, kept in unexpanded form
                 * rather than copying the renderer's optimized equation. */
                blend = opacity * color + (1.0 - opacity) *
                        color * expected[j];
                expected[j] = ((1.0 - ink) * a * expected[j] +
                    ink * ((1.0 - a) * color + a * blend)) / next;
            }
            a = next;
        }
        if (!bgcolor && probe.mask[0][i] != (a == 0.0 ? 1u : 0u)) {
            return 1;
        }
        for (j = 0u; j < 3u; ++j) {
            if (!isfinite(actual[i * 3u + j]) ||
                fabs(actual[i * 3u + j] - expected[j]) > 2e-6) {
                fprintf(stderr, "PSD alpha=%d pixel=%u channel=%u "
                    "%.9g != %.9g\n", specimen, i, j,
                    actual[i * 3u + j], expected[j]);
                return 1;
            }
        }
    }
    return 0;
}
