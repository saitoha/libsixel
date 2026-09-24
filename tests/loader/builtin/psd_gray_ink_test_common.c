/* SPDX-License-Identifier: MIT */
/* Color-record Gray is black ink percentage, not image-sample brightness.
 * Fixed sRGB references distinguish both endpoints and unequal midtones.
 * Each in-memory image has full, partial and zero spot coverage. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "cms.h"
#include "loader_builtin_memory_test_common.h"
#include "psd_gray_ink_test_common.h"

static void
psdg_u32(edge_writer_t *w, unsigned int value)
{
    edge_put_u16be(w, value >> 16);
    edge_put_u16be(w, value & 65535u);
}

static void
psdg_resource(edge_writer_t *w, unsigned int id,
                 unsigned char const *bytes, size_t length)
{
    edge_put_bytes(w, (unsigned char const *)"8BIM", 4u);
    edge_put_u16be(w, id);
    edge_put_u16be(w, 0u);
    psdg_u32(w, (unsigned int)length);
    edge_put_bytes(w, bytes, length);
    if (length & 1u) {
        edge_put_u8(w, 0u);
    }
}

int
psd_gray_ink_test(int specimen)
{
    static unsigned int const gray[] = {
        0u, 10000u, 2500u, 7500u, 1u, 9999u, 10001u, 65535u,
        2500u, 2500u, 2500u, 2500u
    };
    /* Independently evaluated IEC sRGB transfer at 1 - gray / 10000.
     * No production color function or CMS generates these constants. */
    static double const ink[] = {
        1.0, 0.0, 0.5225215539683918, 0.05087608817155679,
        0.9997725269421511, 0.000007739938080495356, 0.0, 0.0,
        0.5225215539683918, 0.5225215539683918,
        0.5225215539683918, 0.5225215539683918
    };
    static double const base[] = {
        0.05126945837404324, 0.21586050011389926, 0.5271151257058131
    };
    unsigned char file[256];
    unsigned char display[20];
    unsigned char alternate[18];
    unsigned char resources[96];
    unsigned char const identifier[] = {0x12, 0x34, 0x56, 0x78};
    unsigned char const samples[] = {0, 128, 255};
    edge_writer_t fw;
    edge_writer_t dw;
    edge_writer_t aw;
    edge_writer_t rw;
    edge_loader_options_t options;
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    float actual[9];
    double expected;
    double paper;
    double v;
    unsigned int process;
    unsigned int j;
    unsigned int k;
    int result;

    if (specimen < 1 || specimen > 12) {
        return 1;
    }
    process = specimen == 11 ? 3u : specimen == 12 ? 1u : 0u;
    fw = (edge_writer_t){file, sizeof(file), 0u, 0};
    dw = (edge_writer_t){display, sizeof(display), 0u, 0};
    aw = (edge_writer_t){alternate, sizeof(alternate), 0u, 0};
    rw = (edge_writer_t){resources, sizeof(resources), 0u, 0};
    if (specimen != 9) {
        psdg_u32(&dw, 1u);
    }
    /* The alternate must replace a conflicting RGB definition. Unused
     * Gray components are deliberately nonzero; only the first is data. */
    edge_put_u16be(&dw, specimen == 10 ? 0u : 8u);
    edge_put_u16be(&dw, specimen == 10 ? 65535u : gray[specimen - 1]);
    edge_put_u16be(&dw, 0u);
    edge_put_u16be(&dw, 65535u);
    edge_put_u16be(&dw, 1234u);
    edge_put_u16be(&dw, 100u);
    edge_put_u8(&dw, 2u);
    if (specimen == 9) {
        edge_put_u8(&dw, 0u);
    }
    psdg_resource(&rw, specimen == 9 ? 1007u : 1077u,
                   display, dw.length);
    if (specimen == 10) {
        edge_put_u16be(&aw, 1u);
        edge_put_u16be(&aw, 1u);
        edge_put_bytes(&aw, identifier, sizeof(identifier));
        edge_put_u16be(&aw, 8u);
        edge_put_u16be(&aw, gray[specimen - 1]);
        edge_put_u16be(&aw, 1234u);
        edge_put_u16be(&aw, 65535u);
        edge_put_u16be(&aw, 4567u);
        psdg_resource(&rw, 1067u, alternate, aw.length);
        psdg_resource(&rw, 1053u, identifier, sizeof(identifier));
    }
    edge_put_bytes(&fw, (unsigned char const *)"8BPS", 4u);
    edge_put_u16be(&fw, 1u);
    for (j = 0u; j < 6u; ++j) {
        edge_put_u8(&fw, 0u);
    }
    edge_put_u16be(&fw, process + 1u);
    psdg_u32(&fw, 1u);
    psdg_u32(&fw, 3u);
    edge_put_u16be(&fw, 8u);
    edge_put_u16be(&fw, process == 0u ? 7u : process);
    psdg_u32(&fw, 0u);
    psdg_u32(&fw, (unsigned int)rw.length);
    edge_put_bytes(&fw, resources, rw.length);
    psdg_u32(&fw, 0u);
    edge_put_u16be(&fw, 0u);
    for (j = 0u; j < process; ++j) {
        for (k = 0u; k < 3u; ++k) {
            edge_put_u8(&fw, (j + 1u) * 64u);
        }
    }
    edge_put_bytes(&fw, samples, sizeof(samples));
    if (fw.failed || dw.failed || aw.failed || rw.failed) {
        return 1;
    }
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    result = edge_load_buffer_options("PSD Gray ink", file, fw.length,
                                       &options, &probe, &status);
    if (specimen == 7 || specimen == 8) {
        return result != 0 || SIXEL_SUCCEEDED(status) ||
            probe.callback_count != 0;
    }
    if (result != 0 || SIXEL_FAILED(status) || probe.callback_count != 1 ||
        probe.width[0] != 3 || probe.height[0] != 1 ||
        probe.pixelformat[0] != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        probe.colorspace[0] != SIXEL_COLORSPACE_LINEAR ||
        probe.rgb_size[0] != sizeof(actual) || probe.mask_size[0] != 0u) {
        fprintf(stderr, "PSD Gray ink case=%d status=%x fmt=%x\n",
            specimen, status, probe.pixelformat[0]);
        return 1;
    }
    memcpy(actual, probe.rgb[0], sizeof(actual));
    for (j = 0u; j < 9u; ++j) {
        v = samples[j / 3u] / 255.0;
        paper = process == 0u ? 1.0 : base[process == 1u ? 0u : j % 3u];
        expected = paper * v + ink[specimen - 1] * (1.0 - v);
        /* Absolute error: 6e-8 covers one float32 rounding near white
         * plus ordinary scalar transfer arithmetic, not CMS error. */
        if (!isfinite(actual[j]) || fabs(actual[j] - expected) > 6e-8) {
            fprintf(stderr, "PSD Gray ink case=%d component=%u "
                "%.9g != %.9g\n", specimen, j, actual[j], expected);
            return 1;
        }
    }
    return 0;
}
