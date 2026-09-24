/* SPDX-License-Identifier: MIT */
/* Process components and spots have distinct color definitions. Build tiny
 * files in memory and compare every output component with scalar/ICC-tag
 * oracles, independently of the production CMS and preview implementation. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "src/cms.h"
#include "loader_builtin_memory_test_common.h"
#include "psd_process_test_common.h"
#include "tests/data/color-numeric/cmyk.h"
#include "tests/data/color-numeric/minimal-profiles.h"

static void
psdp_u32(edge_writer_t *w, uint32_t n)
{
    edge_put_u16be(w, n >> 16);
    edge_put_u16be(w, n & 65535u);
}

static void
psdp_resource(edge_writer_t *w, unsigned int id,
                unsigned char const *p, size_t n)
{
    edge_put_bytes(w, (unsigned char const *)"8BIM", 4u);
    edge_put_u16be(w, id);
    edge_put_u16be(w, 0u);
    psdp_u32(w, (uint32_t)n);
    edge_put_bytes(w, p, n);
    if (n & 1u) {
        edge_put_u8(w, 0u);
    }
}

static double
psdp_linear(double x)
{
    double a;

    a = fabs(x);
    a = a <= 0.04045 ? a / 12.92 : pow((a + 0.055) / 1.055, 2.4);
    return x < 0.0 ? -a : a;
}

int
psd_process_test(int specimen)
{
    static unsigned int const density[8] = {0, 255, 128, 64, 32, 96, 160, 224};
    static double const p3[3] = {
        1.224939473660301, -0.04204725979332884, -0.019642226052377188
    };
    static double const alternate[3] = {
        0.000027866380901354132, 0.33724029701302805, 0.3763792246238528
    };
    static unsigned int const real_ink_words[2][4] = {
        {0x3566u, 0x6338u, 0x3365u, 0u}, {65535u, 0u, 0u, 0u}
    };
    unsigned char file[4096];
    unsigned char resources[2048];
    unsigned char definition[1024];
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
    char const *message;
    double source[8][4];
    double coverage[8];
    double expected[3];
    double sample;
    float value;
    float actual[24];
    uint32_t bits;
    uint32_t s1;
    uint32_t s2;
    unsigned int depth;
    unsigned int version;
    unsigned int compression;
    unsigned int base;
    unsigned int extras;
    unsigned int channels;
    unsigned int spot;
    unsigned int k;
    unsigned int i;
    unsigned int j;
    unsigned int code;
    size_t n;
    size_t row;
    int cmyk;
    int mask;
    int profiled;
    int reject;
    int result;

    cmyk = specimen == 89 || specimen == 91 || specimen == 93 ||
        specimen == 95 || (specimen >= 97 && specimen <= 99) ||
        specimen == 115 || specimen == 125;
    mask = specimen == 90 || specimen == 91 || specimen == 111 ||
        specimen == 112 ||
        (specimen >= 105 && specimen <= 110) ||
        (specimen >= 123 && specimen <= 125);
    profiled = specimen >= 96 && specimen <= 103;
    reject = (specimen >= 101 && specimen <= 103) ||
        (specimen >= 105 && specimen <= 116) ||
        specimen == 121 || specimen == 122 || specimen == 124;
    depth = specimen == 92 || specimen == 93 || specimen == 98 ||
        specimen == 108 || specimen == 110 || specimen == 120 ? 16u :
        specimen == 94 || specimen == 95 || specimen == 99 ||
        specimen == 109 || specimen == 113 || specimen == 114 ||
        specimen == 115 || specimen == 123 ? 32u : 8u;
    version = specimen == 109 || specimen == 120 ? 2u : 1u;
    compression = specimen == 117 || specimen == 120 ? 1u :
        specimen == 118 ? 2u : specimen == 119 ? 3u : 0u;
    base = cmyk ? 4u : 3u;
    extras = specimen == 104 ? 0u : mask || specimen == 126 ? 2u : 1u;
    spot = mask && specimen != 91 && specimen != 111 && specimen != 125
         ? 1u : 0u;
    channels = base + extras;
    fw = (edge_writer_t){file, sizeof(file), 0u, 0};
    rw = (edge_writer_t){resources, sizeof(resources), 0u, 0};
    dw = (edge_writer_t){definition, sizeof(definition), 0u, 0};
    lw = (edge_writer_t){layer, sizeof(layer), 0u, 0};
    pw = (edge_writer_t){raw, sizeof(raw), 0u, 0};
    if (extras != 0u) {
        psdp_u32(&dw, 1u);
        for (k = 0u; k < extras; ++k) {
            /* Saved masks may have an opaque library color: it is not a
             * printable ink and must never enter color interpretation. */
            edge_put_u16be(&dw, specimen == 125 ? (k == spot ? 0x2331u : 0u)
                             : k != spot && specimen != 126 ? 0x2331u : 0u);
            for (j = 0u; j < 4u; ++j) {
                edge_put_u16be(&dw, specimen == 125 ? real_ink_words[k][j] :
                    specimen == 126 && k == 1u ?
                    (j == 0u || j == 2u ? 65535u : 0u) :
                    j == 1u ? 65535u : 0u);
            }
            edge_put_u16be(&dw, specimen == 125 && k == spot ? 0u :
                             specimen == 126 && k == 1u ? 25u : 50u);
            edge_put_u8(&dw, k == spot || specimen == 126 ? 2u :
                            specimen == 90 ? 0u : 1u);
        }
        psdp_resource(&rw, 1077u, definition,
                       dw.length - (specimen == 111 ? 13u : 0u));
        dw.length = 0u;
        for (k = 0u; k < extras; ++k) {
            psdp_u32(&dw, specimen == 105 && k == 0u ? 0u :
                specimen == 124 || k == spot ? 3u :
                specimen == 126 ? 4u : 2u);
        }
        psdp_resource(&rw, 1053u, definition, dw.length);
        if (specimen == 112 || specimen == 125) {
            dw.length = 0u;
            edge_put_u16be(&dw, 1u);
            edge_put_u16be(&dw, 1u);
            psdp_u32(&dw, specimen == 112 ? 2u : 3u);
            edge_put_u16be(&dw, 7u);
            /* Photoshop cmyk-alpha-spot.psd alternate Lab, independently
             * interpreted by the rational D50 oracle retained externally. */
            edge_put_u16be(&dw, 5847u);
            edge_put_u16be(&dw, 65536u - 3327u);
            edge_put_u16be(&dw, 65536u - 1459u);
            edge_put_u16be(&dw, 0u);
            psdp_resource(&rw, 1067u, definition, dw.length);
        }
    }
    if (profiled) {
        if (cmyk || specimen == 101) {
            psdp_resource(&rw, 1039u, color_cmyk_icc, sizeof(color_cmyk_icc));
        } else {
            memcpy(definition, color_minimal_display_p3,
                   sizeof(color_minimal_display_p3));
            if (specimen == 103) {
                memset(definition + 128u, 0, 4u);
            }
            psdp_resource(&rw, 1039u, definition, specimen == 102 ? 8u :
                           sizeof(color_minimal_display_p3));
        }
    }
    if (specimen == 106) {
        /* One empty layer record with a negative count, plus empty global
         * mask. The valid merged pixels remain present after this section. */
        psdp_u32(&lw, 48u);
        edge_put_u16be(&lw, 65535u);
        for (i = 0u; i < 18u; ++i) {
            edge_put_u8(&lw, 0u);
        }
        edge_put_bytes(&lw, (unsigned char const *)"8BIMnorm", 8u);
        psdp_u32(&lw, UINT32_C(0xff000000));
        psdp_u32(&lw, 12u);
        for (i = 0u; i < 16u; ++i) {
            edge_put_u8(&lw, 0u);
        }
    } else if ((specimen >= 107 && specimen <= 110) || specimen == 122) {
        psdp_u32(&lw, 0u);
        if (version == 2u) {
            psdp_u32(&lw, 0u);
        }
        psdp_u32(&lw, 0u);
        edge_put_bytes(&lw, (unsigned char const *)"8BIM", 4u);
        edge_put_bytes(&lw, (unsigned char const *)(specimen == 107 ? "Mtrn" :
            specimen == 108 ? "Mt16" : specimen == 109 ? "Mt32" :
            specimen == 110 ? "Lr16" : "junk"), 4u);
        if (version == 2u) {
            psdp_u32(&lw, 0u);
        }
        psdp_u32(&lw, specimen == 110 ? 2u : specimen == 122 ? 100u : 0u);
        if (specimen == 110) {
            psdp_u32(&lw, UINT32_C(0xffff0000));
        }
    }
    edge_put_bytes(&fw, (unsigned char const *)"8BPS", 4u);
    edge_put_u16be(&fw, version);
    for (i = 0u; i < 6u; ++i) {
        edge_put_u8(&fw, 0u);
    }
    edge_put_u16be(&fw, channels);
    psdp_u32(&fw, 1u);
    psdp_u32(&fw, 8u);
    edge_put_u16be(&fw, depth);
    edge_put_u16be(&fw, cmyk ? 4u : 3u);
    psdp_u32(&fw, 0u);
    psdp_u32(&fw, (uint32_t)rw.length);
    edge_put_bytes(&fw, resources, rw.length);
    if (version == 2u) {
        psdp_u32(&fw, 0u);
    }
    psdp_u32(&fw, (uint32_t)lw.length);
    edge_put_bytes(&fw, layer, lw.length);
    edge_put_u16be(&fw, compression);
    for (k = 0u; k < channels; ++k) {
        for (i = 0u; i < 8u; ++i) {
            sample = k < base ? (cmyk ? (k == 3u ? 224.0 :
                192.0 - k * 64.0) : (k + 1u) * 64.0) / 255.0 :
                k == base + spot ? density[i] / 255.0 :
                specimen == 126 ? density[(i + 3u) % 8u] / 255.0 : 0.0;
            if (k < base && profiled && cmyk) {
                sample = specimen == 98 || specimen == 99 ?
                    1.0 - color_cmyk_source16[i][k] / 65535.0 :
                    1.0 - color_cmyk_source[i][k] / 255.0;
            } else if (k < base && profiled) {
                sample = k == 0u ? 1.0 : 0.0;
            } else if (k < base && specimen == 94) {
                sample = k == 0u ? -0.25 : k == 1u ? 1.25 : 0.5;
            } else if (k < base && specimen == 95 && i == 2u) {
                /* Native float inks must retain information below a byte. */
                sample += 1.0 / 65536.0;
            } else if (k < base && specimen == 125) {
                sample = 1.0;
            }
            if (depth == 8u) {
                code = (unsigned int)(sample * 255.0 + 0.5);
                edge_put_u8(&pw, code);
                sample = code / 255.0;
            } else if (depth == 16u) {
                code = (unsigned int)(sample * 65535.0 + 0.5);
                if ((specimen == 92 || specimen == 93 || specimen == 120) &&
                    i == 2u) {
                    ++code;
                }
                edge_put_u16be(&pw, code);
                sample = code / 65535.0;
            } else {
                value = (float)sample;
                memcpy(&bits, &value, sizeof(bits));
                if ((specimen == 113 && k == base + spot) ||
                    (specimen == 114 && k == 0u) ||
                    (specimen == 123 && k == base)) {
                    bits = UINT32_C(0x7fc00000);
                } else if (specimen == 115 && k == 0u) {
                    bits = UINT32_C(0x3fc00000);
                }
                psdp_u32(&pw, bits);
                sample = value;
            }
            if (k < base) {
                source[i][k] = sample;
            } else if (k == base + spot) {
                coverage[i] = sample;
            }
        }
    }
    row = 8u * (depth / 8u);
    if (compression == 1u) {
        for (k = 0u; k < channels; ++k) {
            if (version == 2u) {
                psdp_u32(&fw, (uint32_t)row + 1u);
            } else {
                edge_put_u16be(&fw, (unsigned int)row + 1u);
            }
        }
        for (k = 0u; k < channels; ++k) {
            edge_put_u8(&fw, (unsigned int)row - 1u);
            edge_put_bytes(&fw, raw + k * row, row);
        }
    } else if (compression >= 2u) {
        if (compression == 3u) {
            for (k = 0u; k < channels; ++k) {
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
        psdp_u32(&fw, s2 * 65536u + s1);
    } else if (specimen != 121) {
        edge_put_bytes(&fw, raw, pw.length);
    }
    if (specimen == 116) {
        --fw.length;
    }
    if (fw.failed || rw.failed || dw.failed || lw.failed || pw.failed) {
        return 1;
    }
    edge_loader_options_init(&options);
    options.require_static = 1;
    if (profiled && specimen != 100) {
        options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;
    }
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    result = edge_load_buffer_options("PSD process spots", file, fw.length,
                                       &options, &probe, &status);
    if (reject) {
        if (result != 0 || SIXEL_SUCCEEDED(status) || probe.callback_count) {
            return 1;
        }
        message = sixel_helper_get_additional_message();
        if (specimen >= 105 && specimen <= 110 && (message == NULL ||
            strstr(message, "merged transparency unsupported") == NULL)) {
            return 1;
        }
        return 0;
    }
    if (result != 0 || SIXEL_FAILED(status) || probe.callback_count != 1 ||
        probe.width[0] != 8 || probe.height[0] != 1 ||
        probe.mask_size[0] != 0u) {
        fprintf(stderr, "PSD process case=%d status=%x callbacks=%d\n",
            specimen, status, probe.callback_count);
        return 1;
    }
    if (specimen == 104) {
        if (probe.pixelformat[0] != SIXEL_PIXELFORMAT_RGB888 ||
            probe.colorspace[0] != SIXEL_COLORSPACE_GAMMA ||
            probe.rgb_size[0] != 24u) {
            return 1;
        }
        for (i = 0u; i < 24u; ++i) {
            if (probe.rgb[0][i] != (i % 3u + 1u) * 64u) {
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
        for (j = 0u; j < 3u; ++j) {
            expected[j] = specimen == 96 ? p3[j] :
                specimen == 98 || specimen == 99 ?
                color_cmyk_linear16[i][j] :
                specimen == 97 ? color_cmyk_linear[i][j] :
                psdp_linear(source[i][j] * (cmyk ? source[i][3] : 1.0));
            sample = coverage[i];
            expected[j] *= sample;
            if (specimen == 125) {
                expected[j] += alternate[j] * (1.0 - sample);
            } else if (j == 1u) {
                expected[j] += (1.0 - sample) *
                    (0.5 + 0.5 * (specimen == 96 ? p3[j] :
                    specimen == 98 || specimen == 99 ?
                    color_cmyk_linear16[i][j] : specimen == 97 ?
                    color_cmyk_linear[i][j] : psdp_linear(
                    source[i][j] * (cmyk ? source[i][3] : 1.0))));
            }
            if (specimen == 126) {
                /* A second magenta ink follows green, with different
                 * solidity and coverage. The reverse order is distinct. */
                sample = density[(i + 3u) % 8u] / 255.0;
                expected[j] = j == 1u ? expected[j] * sample :
                    0.25 * (1.0 - sample) +
                    expected[j] * (0.75 + 0.25 * sample);
            }
            if (!isfinite(actual[i * 3u + j]) ||
                fabs(actual[i * 3u + j] - expected[j]) > 2e-6) {
                fprintf(stderr, "PSD process=%d pixel=%u channel=%u "
                    "%.9g != %.9g\n", specimen, i, j,
                    actual[i * 3u + j], expected[j]);
                return 1;
            }
        }
    }
    return 0;
}
