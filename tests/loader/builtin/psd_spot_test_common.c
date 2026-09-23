/* SPDX-License-Identifier: MIT */
/* Resource 1067 is keyed by ID, not by plane or resource order. Fixtures
 * deliberately disagree in these orders and use distinct plate samples.
 * Fixed display-color expectations use an independent scalar HSV/sRGB
 * oracle; they never call the production renderer or a CMM. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "loader_builtin_memory_test_common.h"
#include "psd_spot_test_common.h"

static void
psds_u32(edge_writer_t *w, uint32_t value)
{
    edge_put_u16be(w, value >> 16);
    edge_put_u16be(w, value & 65535u);
}

static void
psds_resource(edge_writer_t *w, unsigned int id,
                unsigned char const *bytes, size_t length)
{
    edge_put_bytes(w, (unsigned char const *)"8BIM", 4u);
    edge_put_u16be(w, id);
    edge_put_u16be(w, 0u);
    psds_u32(w, (uint32_t)length);
    edge_put_bytes(w, bytes, length);
    if (length & 1u) {
        edge_put_u8(w, 0u);
    }
}

static void
psds_color(edge_writer_t *w, unsigned int space,
             unsigned int const component[4])
{
    unsigned int j;

    edge_put_u16be(w, space);
    for (j = 0u; j < 4u; ++j) {
        edge_put_u16be(w, component[j]);
    }
}

int
psd_spot_test(int specimen)
{
    static unsigned int const colors[14][4] = {
        {0, 65535, 65535, 0}, {10923, 65535, 65535, 0},
        {21845, 65535, 65535, 0}, {32768, 65535, 65535, 0},
        {43690, 65535, 65535, 0}, {54613, 65535, 65535, 0},
        {65535, 65535, 65535, 0}, {40000, 0, 32768, 0},
        {6000, 40000, 50000, 0}, {0, 65535, 65535, 65535},
        {65535, 0, 0, 65535}, {65535, 65535, 65535, 0},
        {65535, 65535, 65535, 32768}, {10000, 30000, 50000, 40000}
    };
    static double const oracle[14][3] = {
        {1.0, 0.0, 0.0}, {0.999895865762368, 1.0, 0.0},
        {0.0, 1.0, 0.0}, {0.0, 0.999895865762368, 1.0},
        {0.0, 0.0, 1.0}, {1.0, 0.0, 0.999895865762368},
        {1.0, 0.0, 0.0},
        {0.214048202298185, 0.214048202298185, 0.214048202298185},
        {0.542924835206199, 0.266504432306607, 0.071897243140175},
        {0.0, 1.0, 1.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, 0.0},
        {0.214048202298185, 0.214048202298185, 0.214048202298185},
        {0.008990280347985, 0.063452634562565, 0.183634905765892}
    };
    static double const lab[3] = {
        0.381583770204388, 0.03923905253633091, 1.1740456243978832
    };
    unsigned char file[4096];
    unsigned char display[800];
    unsigned char identifiers[256];
    unsigned char alternates[1024];
    unsigned char resources[3072];
    edge_writer_t fw;
    edge_writer_t dw;
    edge_writer_t iw;
    edge_writer_t aw;
    edge_writer_t rw;
    edge_loader_options_t options;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    float actual[6];
    double expected[6];
    double v;
    unsigned int component[4];
    uint32_t ids[56];
    unsigned int channels;
    unsigned int space;
    unsigned int alternate_count;
    unsigned int k;
    unsigned int j;
    int simple;
    int reject;
    int result;

    simple = specimen >= 66 && specimen <= 79;
    channels = simple || specimen == 84 ? 1u : specimen == 86 ? 56u : 2u;
    reject = (specimen >= 50 && specimen <= 61) || specimen == 64 ||
        specimen == 65 || specimen == 80 || specimen == 81 || specimen == 85;
    fw = (edge_writer_t){file, sizeof(file), 0u, 0};
    dw = (edge_writer_t){display, sizeof(display), 0u, 0};
    iw = (edge_writer_t){identifiers, sizeof(identifiers), 0u, 0};
    aw = (edge_writer_t){alternates, sizeof(alternates), 0u, 0};
    rw = (edge_writer_t){resources, sizeof(resources), 0u, 0};
    ids[0] = specimen == 63 ? UINT32_C(0xffffffff) : 0x1234u;
    ids[1] = specimen == 53 ? ids[0] : 0xabcdu;
    for (k = 2u; k < channels; ++k) {
        ids[k] = 0x10000u + k;
    }
    if (specimen != 83) {
        psds_u32(&dw, 1u);
    }
    for (k = 0u; k < channels; ++k) {
        psds_u32(&iw, ids[k]);
        memset(component, 0, sizeof(component));
        space = specimen == 49 || specimen == 84 ? 99u : 0u;
        if (simple) {
            space = specimen < 75 ? 1u : 2u;
            memcpy(component, colors[specimen - 66], sizeof(component));
        } else if (specimen == 62 || specimen == 87 ||
                   (specimen == 48 && k == 1u)) {
            component[k == 0u ? 0u : 2u] = 65535u;
        }
        psds_color(&dw, space, component);
        edge_put_u16be(&dw, channels > 1u && k == channels - 1u
                           ? 50u : 0u);
        edge_put_u8(&dw, (specimen == 65 || specimen == 85) && k == 1u
                         ? (specimen == 65 ? 0u : 1u) : 2u);
        if (specimen == 83) {
            edge_put_u8(&dw, 0u);
        }
    }
    if (specimen == 52) {
        iw.length -= 4u;
    } else if (specimen == 60) {
        --iw.length;
    } else if (specimen == 81) {
        for (k = channels; k < 57u; ++k) {
            psds_u32(&iw, k);
        }
    }
    alternate_count = specimen == 62 || specimen == 87 ? 0u :
        specimen == 48 ? 1u : specimen == 80 ? 57u : channels;
    edge_put_u16be(&aw, specimen == 56 ? 2u : 1u);
    edge_put_u16be(&aw, specimen == 57 ? 1u :
                         specimen == 80 ? 57u : alternate_count);
    for (k = 0u; k < alternate_count; ++k) {
        /* Reverse the records while preserving plane order in 1053. A
         * partial table replaces red only, leaving the blue base intact. */
        j = specimen == 48 ? 0u : channels - 1u - k % channels;
        psds_u32(&aw, specimen == 55 ? 999u :
                       specimen == 54 ? ids[0] : ids[j]);
        memset(component, 0, sizeof(component));
        component[j == 0u ? 0u : 2u] = 65535u;
        if (specimen == 86 && j > 0u && j < channels - 1u) {
            component[0] = component[1] = component[2] = 65535u;
        }
        space = specimen == 50 ? 99u : 0u;
        if (specimen == 84) {
            space = 7u;
            component[0] = 5000u;
            component[1] = 7200u;
            component[2] = 65536u - 8800u;
        }
        psds_color(&aw, space, component);
    }
    if (specimen == 58) {
        --aw.length;
    }
    if (specimen == 47) {
        psds_resource(&rw, 1067u, alternates, aw.length);
    }
    if (specimen != 64) {
        psds_resource(&rw, specimen == 83 ? 1007u : 1077u,
                       display, dw.length);
    }
    if (!simple && specimen != 51) {
        psds_resource(&rw, 1053u, identifiers, iw.length);
        if (specimen == 61 || specimen == 87) {
            psds_resource(&rw, 1053u, identifiers, iw.length);
        }
    }
    if (!simple && specimen != 47) {
        psds_resource(&rw, 1067u, alternates, aw.length);
        if (specimen == 59) {
            psds_resource(&rw, 1067u, alternates, aw.length);
        }
    }
    edge_put_bytes(&fw, (unsigned char const *)"8BPS", 4u);
    edge_put_u16be(&fw, specimen == 82 ? 2u : 1u);
    for (k = 0u; k < 6u; ++k) {
        edge_put_u8(&fw, 0u);
    }
    edge_put_u16be(&fw, channels);
    psds_u32(&fw, 1u);
    psds_u32(&fw, 2u);
    edge_put_u16be(&fw, specimen == 82 ? 16u : 8u);
    edge_put_u16be(&fw, 7u);
    psds_u32(&fw, 0u);
    psds_u32(&fw, (uint32_t)rw.length);
    edge_put_bytes(&fw, resources, rw.length);
    psds_u32(&fw, 0u);
    if (specimen == 82) {
        psds_u32(&fw, 0u);
    }
    edge_put_u16be(&fw, 0u);
    for (k = 0u; k < channels; ++k) {
        for (j = 0u; j < 2u; ++j) {
            if (specimen == 82) {
                edge_put_u16be(&fw, k == j ? 0u : 32769u);
            } else {
                edge_put_u8(&fw, (k == 0u && j == 0u) ||
                                  (k == channels - 1u && j == 1u &&
                                   channels > 1u) ? 0u : 128u);
            }
        }
    }
    if (fw.failed || dw.failed || iw.failed || aw.failed || rw.failed) {
        return 1;
    }
    edge_loader_options_init(&options);
    options.require_static = 1;
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    result = edge_load_buffer_options("PSD alternate spots", file, fw.length,
                                       &options, &probe, &status);
    if (reject) {
        return result != 0 || SIXEL_SUCCEEDED(status) ||
            probe.callback_count != 0;
    }
    if (result != 0 || SIXEL_FAILED(status) || probe.callback_count != 1 ||
        probe.width[0] != 2 || probe.height[0] != 1 ||
        probe.pixelformat[0] != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        probe.colorspace[0] != SIXEL_COLORSPACE_LINEAR ||
        probe.rgb_size[0] != sizeof(actual) || probe.mask_size[0] != 0u) {
        fprintf(stderr, "PSD spot case=%d status=%x fmt=%x bytes=%lu\n",
            specimen, status, probe.pixelformat[0],
            (unsigned long)probe.rgb_size[0]);
        return 1;
    }
    memcpy(actual, probe.rgb[0], sizeof(actual));
    v = specimen == 82 ? 32769.0 / 65535.0 : 128.0 / 255.0;
    if (simple || specimen == 84) {
        for (j = 0u; j < 3u; ++j) {
            expected[j] = simple ? oracle[specimen - 66][j] : lab[j];
            expected[j + 3u] = v + (1.0 - v) * expected[j];
        }
    } else {
        /* Red then half-solid blue, with complementary plate samples.
         * This is asymmetric in color, coverage, order, and solidity. */
        expected[0] = v;
        expected[1] = 0.0;
        expected[2] = (1.0 - v) * 0.5;
        expected[3] = expected[4] = 0.0;
        expected[5] = (1.0 + v) * 0.5;
    }
    for (j = 0u; j < 6u; ++j) {
        if (!isfinite(actual[j]) || fabs(actual[j] - expected[j]) > 2e-6) {
            fprintf(stderr, "PSD spot case=%d component=%u %.9g != %.9g\n",
                specimen, j, actual[j], expected[j]);
            return 1;
        }
    }
    return 0;
}
