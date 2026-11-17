/* SPDX-License-Identifier: MIT */
#ifndef CMS_FORMAT_ADAPTER_COMMON_H
#define CMS_FORMAT_ADAPTER_COMMON_H
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/gray-samples.h"

static int
cms_format_adapter_test(int mode)
{
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_transform_t *transform;
    sixel_cms_engine_t previous;
    sixel_cms_pixel_format_t format;
    sixel_cms_pixel_format_t destination;
    sixel_cms_status_t status;
    unsigned char input[128];
    unsigned char original[128];
    unsigned char packed[128];
    uint16_t words[8] = {32768, 32769, 65535, 16384,
                          1, 2, 3, 65535};
    float floats[8] = {0.25f, 0.125f, 0.0f, 0.5f,
                       0.1f, 0.2f, 0.3f, 0.0f};
    float output[8];
    double expected[8];
    size_t i;
    size_t count;
    size_t channels;
    int flags;
    int ok;

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    source = mode == 2 ? sixel_cms_open_profile_from_mem(
        color_gray_icc, sizeof(color_gray_icc))
        : mode == 4 ? sixel_cms_create_srgb_profile()
                    : sixel_cms_create_linear_srgb_profile();
    target = sixel_cms_create_linear_srgb_profile();
    memset(input, 0, sizeof(input));
    memset(output, 0x5a, sizeof(output));
    memset(packed, 0x5a, sizeof(packed));
    format = SIXEL_CMS_PIXELFORMAT_RGB_16;
    destination = SIXEL_CMS_PIXELFORMAT_RGB_F32;
    count = 1u;
    channels = 3u;
    flags = SIXEL_CMS_TRANSFORM_COPY_ALPHA;
    memcpy(input, words, sizeof(words));
    for (i = 0u; i < 8u; ++i) {
        expected[i] = words[i] / 65535.0;
    }
    if (mode == 1) {
        format = SIXEL_CMS_PIXELFORMAT_RGBA_16;
        destination = SIXEL_CMS_PIXELFORMAT_RGBA_F32;
        count = 2u;
        channels = 4u;
    } else if (mode == 2) {
        format = SIXEL_CMS_PIXELFORMAT_GRAY_16;
        /* The independent gray fixture has a gamma-two TRC. */
        for (i = 0u; i < 3u; ++i) {
            expected[i] = pow(words[0] / 65535.0, 2.0);
        }
    } else if (mode >= 3 && mode <= 6) {
        format = SIXEL_CMS_PIXELFORMAT_RGBA_F32;
        destination = SIXEL_CMS_PIXELFORMAT_RGBA_F32;
        channels = 4u;
        count = 2u;
        memcpy(input, floats, sizeof(floats));
        for (i = 0u; i < 8u; ++i) { expected[i] = floats[i]; }
        if (mode == 4) {
            flags |= SIXEL_CMS_TRANSFORM_SRC_PREMULTIPLIED;
            /* IEC sRGB decoding of straight 0.5 and 0.25. */
            expected[0] = 0.21404114048223255;
            expected[1] = 0.05087608817155679;
            expected[4] = expected[5] = expected[6] = 0.0;
        }
        if (mode == 5) {
            floats[7] = NAN;
            memcpy(input, floats, sizeof(floats));
        }
        if (mode == 6) {
            count = (size_t)-1;
        }
    } else if (mode == 7 || mode == 8) {
        format = mode == 7 ? SIXEL_CMS_PIXELFORMAT_BGR_8
                          : SIXEL_CMS_PIXELFORMAT_BGRA_8;
        input[0] = 17; input[1] = 96; input[2] = 233; input[3] = 123;
        expected[0] = 233.0 / 255.0;
        expected[1] = 96.0 / 255.0;
        expected[2] = 17.0 / 255.0;
        if (mode == 8) {
            destination = SIXEL_CMS_PIXELFORMAT_RGBA_F32;
            channels = 4u;
            expected[3] = 123.0 / 255.0;
        }
    } else if (mode == 9) {
        destination = SIXEL_CMS_PIXELFORMAT_RGBA_16;
        channels = 4u;
    }
    memcpy(original, input, sizeof(input));
    transform = sixel_cms_create_transform(source, format, target,
                                             destination, flags);
    ok = source != NULL && target != NULL && transform != NULL;
    if (ok) {
        ok = sixel_cms_do_transform_ex(transform, input,
            mode == 0 ? input : mode == 9 ? packed : (void *)output,
            count, &status);
        if (mode == 5 || mode == 6) {
            ok = !ok && memcmp(output, packed, sizeof(output)) == 0 &&
                status == (mode == 5 ? SIXEL_CMS_STATUS_EVALUATION_FAILED
                                     : SIXEL_CMS_STATUS_INVALID_ARGUMENT);
        } else if (mode == 9) {
            memcpy(words, packed, sizeof(words));
            ok = ok && words[0] == 32768 && words[1] == 32769 &&
                words[2] == 65535 && words[3] == 65535;
        } else {
            if (mode == 0) { memcpy(output, input, 3u * sizeof(float)); }
            for (i = 0u; i < count * channels; ++i) {
                ok = ok && isfinite(output[i]) &&
                    fabs(output[i] - expected[i]) < 4e-6;
            }
        }
        if (mode != 0) {
            ok = ok && memcmp(input, original, sizeof(input)) == 0;
        }
    }
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(target);
    sixel_cms_close_profile(source);
    sixel_cms_set_engine(previous);
    return ok ? 0 : 1;
}
#endif
