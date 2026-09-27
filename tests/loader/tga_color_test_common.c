/* SPDX-License-Identifier: MIT */
/* Tiny synthetic files and analytic values, independent of the TGA parser,
 * shared CMS, and byte round-trip expectations. No fixture I/O is needed. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "src/cms.h"
#include "src/loader-common.h"
#include "tests/loader/builtin/loader_builtin_tga_test_common.h"
#include "tests/loader/tga_color_test_common.h"

static void
tgac_word(unsigned char *p, unsigned int value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static void
tgac_offset(unsigned char *p, unsigned int value)
{
    tgac_word(p, value);
    tgac_word(p + 2, value >> 16);
}

static unsigned int
tgac_table(unsigned int index, unsigned int channel)
{
    /* Nonidentical, nonmonotone channels expose ARGB/RGBA confusion,
     * channel-index sharing, early byte rounding, and LUT inversion. */
    if (channel == 0u) {
        return index == 128u ? 32769u : index * 257u;
    }
    if (channel == 1u) {
        return index * 251u + 17u;
    }
    if (channel == 2u) {
        return (255u - index) * 257u;
    }
    return index * index + index + 1u;
}

static double
tgac_linear(double value)
{
    return value <= 0.04045 ? value / 12.92 :
        pow((value + 0.055) / 1.055, 2.4);
}

int
tga_color_test(int specimen, int engine)
{
    static unsigned char const source[4][4] = {
        {64u, 128u, 192u, 255u}, {17u, 93u, 251u, 128u},
        {233u, 31u, 127u, 0u}, {0u, 255u, 255u, 255u}
    };
    unsigned char data[4096];
    unsigned char samples[4][4];
    edge_writer_t writer;
    edge_loader_options_t options;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    sixel_cms_engine_t previous_engine;
    int previous_policy;
    size_t extension;
    size_t table_offset;
    size_t footer;
    size_t i;
    size_t c;
    unsigned int gamma_num;
    unsigned int gamma_den;
    unsigned int packed;
    unsigned int v;
    int indexed;
    int gray;
    int rgb16;
    int has_alpha;
    int use_table;
    int rle;
    int index16;
    int unmanaged;
    int reject;
    int result;
    double expected;
    double alpha;
    double tolerance;
    float actual[12];

    previous_engine = sixel_cms_get_engine();
    sixel_cms_set_engine((sixel_cms_engine_t)engine);
    result = (int)sixel_cms_get_engine() == engine;
    sixel_cms_set_engine(previous_engine);
    if (!result) {
        return 77;
    }
    indexed = specimen == 7 || specimen == 8 || specimen == 15;
    gray = specimen == 5 || specimen == 16 || specimen == 37;
    rgb16 = specimen == 6;
    index16 = specimen == 8;
    rle = specimen == 4 || specimen == 14 || specimen == 30;
    has_alpha = specimen == 9 || specimen == 10 || specimen == 15 ||
        specimen == 17 || specimen == 31 || specimen == 37;
    use_table = (specimen >= 11 && specimen <= 17) ||
        (specimen >= 26 && specimen <= 28) || specimen == 38;
    unmanaged = specimen >= 18 && specimen <= 21;
    reject = specimen >= 22 && specimen <= 31;
    gamma_num = specimen == 2 ? 1u : specimen == 3 ? 22u : 2u;
    gamma_den = specimen == 3 ? 10u : specimen == 20 ? 0u : 1u;
    if (use_table && specimen != 12) {
        gamma_den = 0u;
    }
    memset(data, 0, sizeof(data));
    memcpy(samples, source, sizeof(samples));
    for (i = 0u; i < 4u; ++i) {
        if (gray) {
            samples[i][1] = samples[i][2] = samples[i][0];
        }
        if (!has_alpha) {
            samples[i][3] = 255u;
        }
    }
    writer = (edge_writer_t){ data, sizeof(data), 0u, 0 };
    edge_tga_begin(&writer, indexed ? 1u : 0u,
        (indexed ? 1u : gray ? 3u : 2u) + (rle ? 8u : 0u),
        indexed ? 4u : 0u, indexed ? (has_alpha ? 32u : 24u) : 0u,
        2u, 2u, indexed ? (index16 ? 16u : 8u) :
            gray ? (has_alpha ? 16u : 8u) :
            rgb16 ? 16u : has_alpha ? 32u : 24u, 0x20u);
    if (indexed) {
        tgac_word(data + 3, index16 ? 342u : 7u);
        for (i = 0u; i < 4u; ++i) {
            for (c = 0u; c < 3u; ++c) {
                edge_put_u8(&writer, samples[i][2u - c]);
            }
            if (has_alpha) {
                edge_put_u8(&writer, samples[i][3]);
            }
        }
    }
    if (rle) {
        /* A two-pixel run followed by two raw pixels exercises both packet
         * lengths in the metadata/raster boundary validator. */
        memcpy(samples[1], samples[0], sizeof(samples[0]));
        edge_put_u8(&writer, 0x81u);
    }
    for (i = 0u; i < 4u; ++i) {
        if (rle && i == 1u) {
            continue;
        }
        if (rle && i == 2u) {
            edge_put_u8(&writer, 1u);
        }
        if (indexed) {
            if (index16) {
                edge_put_u16le(&writer, (unsigned int)i + 342u);
            } else {
                edge_put_u8(&writer, (unsigned int)i + 7u);
            }
        } else if (rgb16) {
            packed = ((unsigned int)samples[i][0] >> 3) << 10;
            packed |= ((unsigned int)samples[i][1] >> 3) << 5;
            packed |= (unsigned int)samples[i][2] >> 3;
            edge_put_u16le(&writer, packed);
            for (c = 0u; c < 3u; ++c) {
                samples[i][c] = (unsigned char)(
                    ((unsigned int)samples[i][c] >> 3) * 255u / 31u);
            }
        } else if (gray) {
            edge_put_u8(&writer, samples[i][0]);
            if (has_alpha) {
                edge_put_u8(&writer, samples[i][3]);
            }
        } else {
            for (c = 0u; c < 3u; ++c) {
                edge_put_u8(&writer, samples[i][2u - c]);
            }
            if (has_alpha) {
                edge_put_u8(&writer, samples[i][3]);
            }
        }
    }
    if (specimen == 29 || specimen == 30) {
        --writer.length; /* Metadata must not fill the missing raster byte. */
    }
    extension = writer.length;
    table_offset = use_table ? extension + 495u : 0u;
    if (specimen == 13) {
        table_offset = extension;
        extension += 2048u;
    }
    tgac_word(data + extension, 495u);
    tgac_word(data + extension + 478u, gamma_num);
    tgac_word(data + extension + 480u, gamma_den);
    tgac_offset(data + extension + 482u, (unsigned int)table_offset);
    data[extension + 494u] = has_alpha ? 3u : 0u;
    footer = extension + 495u;
    if (use_table) {
        for (v = 0u; v < 256u; ++v) {
            for (c = 0u; c < 4u; ++c) {
                tgac_word(data + table_offset + v * 8u + c * 2u,
                           tgac_table(v, (unsigned int)c));
            }
        }
        if (specimen != 13) {
            footer += 2048u;
        }
        if (specimen == 38) {
            tgac_word(data + table_offset + 255u * 8u, 0u);
        }
    }
    tgac_offset(data + footer, (unsigned int)extension);
    memcpy(data + footer + 8u, "TRUEVISION-XFILE.\0", 18u);
    writer.length = footer + 26u;
    if (specimen == 19) {
        writer.length = extension; /* Exact original-format byte path. */
    } else if (specimen == 21) {
        tgac_offset(data + footer, 0u);
    } else if (specimen == 22 || specimen == 23) {
        tgac_word(data + extension + 478u, specimen == 22 ? 0u : 11u);
    } else if (specimen == 24) {
        tgac_offset(data + footer, 0xfffffff0u);
    } else if (specimen == 25) {
        tgac_word(data + extension, 494u);
    } else if (specimen == 26) {
        tgac_offset(data + extension + 482u, 0xfffffff0u);
    } else if (specimen == 27) {
        tgac_offset(data + extension + 482u, (unsigned int)extension + 8u);
    } else if (specimen == 28) {
        tgac_offset(data + extension + 482u, (unsigned int)table_offset + 1u);
    } else if (specimen == 31) {
        data[extension + 494u] = 4u;
    }
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.cms_engine = specimen == 18 ? SIXEL_CMS_ENGINE_NONE : engine;
    options.use_palette = indexed;
    options.set_prefer_float32 = 1;
    options.prefer_float32 = 0; /* CMS must override a byte preference. */
    if (specimen == 10 || specimen == 17) {
        options.set_bgcolor = 1;
        options.bgcolor[0] = 51u;
        options.bgcolor[1] = 102u;
        options.bgcolor[2] = 153u;
    }
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    previous_policy = loader_transparent_policy();
    sixel_helper_set_loader_transparent_policy(SIXEL_ALPHA_POLICY_COMPOSITE);
    result = edge_load_buffer_options("TGA color", data, writer.length,
                                       &options, &probe, &status);
    sixel_helper_set_loader_transparent_policy(previous_policy);
    if (reject) {
        if (SIXEL_FAILED(status) && probe.callback_count == 0) {
            return 0;
        }
        fprintf(stderr, "TGA invalid case %d accepted: %x/%d\n",
                specimen, status, probe.callback_count);
        return 1;
    }
    if (result != 0 || SIXEL_FAILED(status) || probe.callback_count != 1 ||
        probe.width[0] != 2 || probe.height[0] != 2 ||
        probe.pixelformat[0] != (unmanaged ? SIXEL_PIXELFORMAT_RGB888 :
            specimen == 36 ? SIXEL_PIXELFORMAT_RGBFLOAT32 :
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) ||
        probe.colorspace[0] != (unmanaged || specimen == 36 ?
            SIXEL_COLORSPACE_GAMMA :
            SIXEL_COLORSPACE_LINEAR)) {
        fprintf(stderr, "TGA case=%d status=%x callbacks=%d type=%x space=%d\n",
                specimen, status, probe.callback_count,
                probe.pixelformat[0], probe.colorspace[0]);
        return 1;
    }
    if (unmanaged) {
        for (i = 0u; i < 4u; ++i) {
            if (memcmp(probe.rgb[0] + i * 3u, samples[i], 3u) != 0) {
                return 1;
            }
        }
        return 0;
    }
    if (probe.rgb_size[0] != sizeof(actual)) {
        return 1;
    }
    memcpy(actual, probe.rgb[0], sizeof(actual));
    /* Native matrix/TRC profiles include fixed-point coefficient rounding.
     * Even their looser budget is well below one byte step; the builtin
     * table oracle separately protects every meaningful 16-bit LUT bit. */
    tolerance = engine == SIXEL_CMS_ENGINE_BUILTIN ? 2e-6 : 3e-4;
    for (i = 0u; i < 4u; ++i) {
        alpha = (double)samples[i][3] / 255.0;
        if (use_table && has_alpha) {
            alpha = (double)tgac_table(samples[i][3], 0u) / 65535.0;
        }
        for (c = 0u; c < 3u; ++c) {
            expected = use_table ?
                tgac_linear((double)tgac_table(samples[i][c],
                             (unsigned int)c + 1u) / 65535.0) :
                pow((double)samples[i][c] / 255.0,
                    (double)gamma_num / (double)gamma_den);
            if (options.set_bgcolor) {
                expected = expected * alpha +
                    tgac_linear((double)options.bgcolor[c] / 255.0) *
                    (1.0 - alpha);
            }
            if (specimen == 36) {
                /* Gamma frame storage is encoded sRGB, distinct from the
                 * source exponent recorded in the TGA extension. */
                expected = expected <= 0.0031308 ? 12.92 * expected :
                    1.055 * pow(expected, 1.0 / 2.4) - 0.055;
            }
            if (!isfinite(actual[i * 3u + c]) ||
                fabs(actual[i * 3u + c] - expected) > tolerance) {
                fprintf(stderr, "TGA case=%d i=%zu c=%zu %.9g != %.9g\n",
                        specimen, i, c, actual[i * 3u + c], expected);
                return 1;
            }
        }
    }
    if (has_alpha && !options.set_bgcolor) {
        if (probe.mask_size[0] != 4u || !probe.alpha_zero_is_transparent[0] ||
            memcmp(probe.mask[0], "\0\0\1\0", 4u) != 0) {
            fprintf(stderr, "TGA zero alpha mask mismatch\n");
            return 1;
        }
    } else if (probe.mask_size[0] != 0u) {
        return 1;
    }
    return 0;
}
