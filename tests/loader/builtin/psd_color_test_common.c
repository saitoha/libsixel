/* SPDX-License-Identifier: MIT */
/* Build tiny independent PSD resources and native planar sample streams.
 * Numeric expectations use neutral Lab identities or elementary primary
 * ink equations, never the production color conversion implementation. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "src/cms.h"
#include "loader_builtin_memory_test_common.h"
#include "psd_color_test_common.h"

static void
psdt_u32(edge_writer_t *w, uint32_t value)
{
    edge_put_u16be(w, value >> 16);
    edge_put_u16be(w, value & 65535u);
}

static void
psdt_resource(edge_writer_t *w, unsigned int id,
                unsigned char const *data, size_t length)
{
    edge_put_bytes(w, (unsigned char const *)"8BIM", 4u);
    edge_put_u16be(w, id);
    edge_put_u16be(w, 0u);
    psdt_u32(w, (uint32_t)length);
    edge_put_bytes(w, data, length);
    if (length & 1u) {
        edge_put_u8(w, 0u);
    }
}

static double
psdt_gray(double index)
{
    double l;

    l = index * (100.0 / 255.0);
    return l > 8.0 ? pow((l + 16.0) / 116.0, 3.0) :
        l * (27.0 / 24389.0);
}

int
psd_color_test(int specimen)
{
    static unsigned int const codes[8] = {0, 255, 128, 64, 32, 96, 160, 224};
    unsigned char file[8192];
    unsigned char resources[4096];
    unsigned char definition[1024];
    unsigned char native[524];
    unsigned char raw[2048];
    edge_writer_t fw;
    edge_writer_t rw;
    edge_writer_t dw;
    edge_writer_t pw;
    edge_loader_options_t options;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    float actual[24];
    double sample[8];
    double expected[3];
    double coordinate;
    double fraction;
    double alpha;
    float value;
    uint32_t bits;
    uint32_t s1;
    uint32_t s2;
    unsigned int version;
    unsigned int depth;
    unsigned int compression;
    unsigned int channels;
    unsigned int code;
    unsigned int ink;
    unsigned int j;
    unsigned int low;
    unsigned int i;
    size_t row_bytes;
    size_t rows;
    size_t offset;
    size_t n;
    int duotone;
    int reject;
    int result;

    version = (specimen == 5 || specimen == 23) ? 2u : 1u;
    depth = (specimen == 6 || specimen == 18) ? 16u :
        (specimen == 7 || specimen == 19 || specimen == 34 ||
         specimen == 35) ? 32u : 8u;
    compression = (specimen == 2 || specimen == 5 || specimen == 20 ||
                   specimen == 23) ? 1u :
        (specimen == 3 || specimen == 21) ? 2u :
        (specimen == 4 || specimen == 22) ? 3u : 0u;
    duotone = specimen <= 11 || (specimen >= 26 && specimen <= 28) ||
        specimen == 39 || specimen == 44 || specimen == 46 || specimen == 47;
    channels = duotone ? ((specimen == 9 || specimen == 10) ? 2u : 1u) :
        (specimen == 12 || specimen == 41 || specimen == 42 ||
         specimen == 45) ? 1u : specimen == 13 ? 2u : specimen == 14 ? 3u :
        specimen == 15 ? 4u : specimen == 17 ? 56u : 5u;
    reject = (specimen >= 26 && specimen <= 36) ||
        (specimen >= 38 && specimen <= 40) ||
        specimen == 44 || specimen == 45;
    fw = (edge_writer_t){file, sizeof(file), 0u, 0};
    rw = (edge_writer_t){resources, sizeof(resources), 0u, 0};
    dw = (edge_writer_t){definition, sizeof(definition), 0u, 0};
    pw = (edge_writer_t){raw, sizeof(raw), 0u, 0};
    if (duotone) {
        edge_put_u16be(&dw, specimen == 28 ? 2u : 1u);
        edge_put_u16be(&dw, specimen == 27 ? 0u : 1u);
        for (i = 0u; i < 10u; ++i) {
            edge_put_u8(&dw, 0u);
        }
        edge_put_u16be(&dw, 256u);
        for (i = 0u; i < 256u; ++i) {
            edge_put_u8(&dw, specimen == 8 ? 128u : i);
            edge_put_u8(&dw, specimen == 8 ? 200u : 128u);
            edge_put_u8(&dw, specimen == 8 ? 40u : 128u);
        }
        if (specimen == 26) {
            --dw.length;
        }
        psdt_resource(&rw, 1066u, definition, dw.length);
        if (specimen == 39) {
            psdt_resource(&rw, 1066u, definition, dw.length);
        }
        if (specimen == 44) {
            dw.length = 0u;
            psdt_u32(&dw, 1u);
            for (i = 0u; i < 12u; ++i) {
                edge_put_u8(&dw, 0u);
            }
            edge_put_u8(&dw, 2u);
            psdt_resource(&rw, 1077u, definition, dw.length);
        }
    } else {
        if (specimen != 24) {
            psdt_u32(&dw, 1u);
        }
        for (ink = 0u; ink < channels; ++ink) {
            edge_put_u16be(&dw, specimen == 33 ? 99u :
                (specimen == 41 || specimen == 45) ? 7u :
                specimen == 42 ? 8u : 0u);
            for (j = 0u; j < 4u; ++j) {
                code = j == 3u ? 0u : 65535u;
                if (ink == 0u && j != 0u) {
                    code = 0u;
                } else if (ink == channels - 1u && channels > 1u &&
                           j != 2u) {
                    code = 0u;
                }
                if (specimen == 41 || specimen == 45) {
                    code = j == 0u ? 5000u : j == 1u ? 7200u :
                        j == 2u ? 65536u - 8800u : 0u;
                    if (specimen == 45 && j == 0u) {
                        code = 10001u;
                    }
                } else if (specimen == 42) {
                    code = j == 0u ? 5000u : 0u;
                }
                edge_put_u16be(&dw, code);
            }
            edge_put_u16be(&dw, specimen == 30 ? 101u :
                (specimen == 25 && ink == channels - 1u) ? 100u : 0u);
            edge_put_u8(&dw, specimen == 31 ? 3u : 2u);
            if (specimen == 24) {
                edge_put_u8(&dw, 0u);
            }
        }
        if (specimen == 29) {
            --dw.length;
        } else if (specimen == 32) {
            dw.length -= 13u;
        }
        if (specimen == 37) {
            /* An obsolete incompatible count must not override 1077. */
            psdt_resource(&rw, 1007u, definition + 4u, 14u);
        }
        psdt_resource(&rw, specimen == 24 ? 1007u : 1077u,
                       definition, dw.length);
        if (specimen == 43) {
            psdt_resource(&rw, 1007u, definition + 4u, 14u);
        }
        if (specimen == 40) {
            psdt_resource(&rw, 1077u, definition, dw.length);
        }
        if (specimen == 38) {
            psdt_resource(&rw, 999u, definition, 1u);
            --rw.length;
        }
    }
    edge_put_bytes(&fw, (unsigned char const *)"8BPS", 4u);
    edge_put_u16be(&fw, version);
    for (i = 0u; i < 6u; ++i) {
        edge_put_u8(&fw, 0u);
    }
    edge_put_u16be(&fw, channels);
    psdt_u32(&fw, 2u);
    psdt_u32(&fw, 4u);
    edge_put_u16be(&fw, depth);
    edge_put_u16be(&fw, duotone ? 8u : 7u);
    if (specimen == 46 || specimen == 47) {
        /* Observed native Duotone blocks match the fixed ADO layout:
         * header, four colors, four names, four curves, dot gain and
         * eleven overprints. This conflicting blue ink and constant
         * full-density curve must not recolor the saved Lab appearance.
         * A future native renderer must also leave unfamiliar opaque
         * variants alone when a usable rendered table is present. */
        memset(native, 0, sizeof(native));
        native[1] = specimen == 46 ? 1u : 255u;
        native[3] = 1u;
        native[10] = native[11] = 255u;
        native[44] = 4u;
        memcpy(native + 45u, "Blue", 4u);
        for (ink = 0u; ink < 4u; ++ink) {
            for (i = 0u; i < 13u; ++i) {
                code = ink == 0u || i == 12u ? 1000u :
                    i == 0u ? 0u : 65535u;
                offset = 300u + ink * 28u + i * 2u;
                native[offset] = (unsigned char)(code >> 8);
                native[offset + 1u] = (unsigned char)code;
            }
        }
        native[413] = 20u;
        for (i = 0u; i < 11u; ++i) {
            native[414u + i * 10u] = 255u;
            native[415u + i * 10u] = 255u;
        }
        psdt_u32(&fw, (uint32_t)sizeof(native));
        edge_put_bytes(&fw, native, sizeof(native));
    } else {
        psdt_u32(&fw, 0u);
    }
    psdt_u32(&fw, (uint32_t)rw.length);
    edge_put_bytes(&fw, resources, rw.length);
    psdt_u32(&fw, 0u);
    if (version == 2u) {
        psdt_u32(&fw, 0u);
    }
    edge_put_u16be(&fw, compression);
    for (ink = 0u; ink < channels; ++ink) {
        for (i = 0u; i < 8u; ++i) {
            code = codes[i];
            if (depth == 8u) {
                edge_put_u8(&pw, code);
                sample[i] = code / 255.0;
            } else if (depth == 16u) {
                code = code * 257u + (i == 2u ? 1u : 0u);
                edge_put_u16be(&pw, code);
                sample[i] = code / 65535.0;
            } else {
                value = (float)(code / 255.0);
                if (i == 2u) {
                    value = 0.5f;
                }
                memcpy(&bits, &value, sizeof(bits));
                if (specimen == 34 && i == 2u) {
                    bits = UINT32_C(0x7fc00000);
                } else if (specimen == 35 && i == 2u) {
                    bits = UINT32_C(0xbf000000);
                }
                psdt_u32(&pw, bits);
                sample[i] = value;
            }
        }
    }
    row_bytes = 4u * (depth / 8u);
    rows = 2u * channels;
    if (compression == 1u) {
        for (n = 0u; n < rows; ++n) {
            if (version == 2u) {
                psdt_u32(&fw, (uint32_t)row_bytes + 1u);
            } else {
                edge_put_u16be(&fw, (unsigned int)row_bytes + 1u);
            }
        }
        for (n = 0u; n < rows; ++n) {
            edge_put_u8(&fw, (unsigned int)row_bytes - 1u);
            edge_put_bytes(&fw, raw + n * row_bytes, row_bytes);
        }
    } else if (compression >= 2u) {
        if (compression == 3u) {
            for (n = 0u; n < rows; ++n) {
                for (offset = row_bytes - 1u; offset > 0u; --offset) {
                    raw[n * row_bytes + offset] -=
                        raw[n * row_bytes + offset - 1u];
                }
            }
        }
        /* A zlib stream with one stored DEFLATE block, independent of any
         * optional compression library on the test host. */
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
        psdt_u32(&fw, s2 * 65536u + s1);
    } else {
        edge_put_bytes(&fw, raw, pw.length);
    }
    if (specimen == 36) {
        --fw.length;
    }
    if (fw.failed || rw.failed || dw.failed || pw.failed) {
        return 1;
    }
    edge_loader_options_init(&options);
    options.require_static = 1;
    if (specimen == 11) {
        options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;
    }
    if (specimen == 10) {
        options.set_bgcolor = 1;
        memset(options.bgcolor, 255, sizeof(options.bgcolor));
    }
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    result = edge_load_buffer_options("PSD color resources", file, fw.length,
                                       &options, &probe, &status);
    if (reject) {
        return result != 0 || SIXEL_SUCCEEDED(status) ||
            probe.callback_count != 0;
    }
    if (result != 0 || SIXEL_FAILED(status) || probe.callback_count != 1 ||
        probe.width[0] != 4 || probe.height[0] != 2 ||
        probe.pixelformat[0] != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        probe.colorspace[0] != SIXEL_COLORSPACE_LINEAR ||
        probe.rgb_size[0] != sizeof(actual)) {
        fprintf(stderr, "PSD case=%d status=%x fmt=%x space=%d bytes=%lu\n",
            specimen, status, probe.pixelformat[0], probe.colorspace[0],
            (unsigned long)probe.rgb_size[0]);
        return 1;
    }
    memcpy(actual, probe.rgb[0], sizeof(actual));
    if (specimen != 9 && probe.mask_size[0] != 0u) {
        return 1;
    }
    for (i = 0u; i < 8u; ++i) {
        if (duotone) {
            coordinate = sample[i] * 255.0;
            low = (unsigned int)coordinate;
            fraction = coordinate - low;
            expected[0] = psdt_gray(low) * (1.0 - fraction) +
                psdt_gray(low == 255u ? low : low + 1u) * fraction;
            expected[1] = expected[2] = expected[0];
            if (specimen == 8) {
                /* Independent D50 Lab(50.196078,72,-88) reference. */
                expected[0] = 0.384636209521255;
                expected[1] = 0.040098161579141464;
                expected[2] = 1.1798441803872168;
            }
            if (specimen == 10) {
                alpha = sample[i];
                for (j = 0u; j < 3u; ++j) {
                    expected[j] = expected[j] * alpha + 1.0 - alpha;
                }
            }
        } else if (channels == 1u) {
            expected[0] = 1.0;
            expected[1] = expected[2] = sample[i];
            if (specimen == 41 || specimen == 42) {
                expected[0] = 0.381583770204388;
                expected[1] = 0.03923905253633091;
                expected[2] = 1.1740456243978832;
                for (j = 0u; j < 3u; ++j) {
                    if (specimen == 42) {
                        expected[j] = 0.21404114048223255;
                    }
                    expected[j] = sample[i] +
                        (1.0 - sample[i]) * expected[j];
                }
            }
        } else {
            expected[0] = expected[2] = sample[i];
            expected[1] = sample[i] * sample[i];
            if (specimen == 25) {
                expected[2] = 1.0 - sample[i] + sample[i] * sample[i];
            }
        }
        for (j = 0u; j < 3u; ++j) {
            if (!isfinite(actual[i * 3u + j]) ||
                fabs(actual[i * 3u + j] - expected[j]) > 2e-6) {
                fprintf(stderr, "PSD case=%d pixel=%u channel=%u "
                    "%.9g != %.9g\n", specimen, i, j,
                    actual[i * 3u + j], expected[j]);
                return 1;
            }
        }
        if (specimen == 9 && (probe.mask_size[0] != 8u ||
            probe.mask[0][i] != (codes[i] == 0u ? 1u : 0u))) {
            return 1;
        }
    }
    return 0;
}
