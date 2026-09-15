/* SPDX-License-Identifier: MIT */
/* Observe the original TIFF ICC specimens against independently generated
 * formulas. The test never constructs expectations with a production CMM. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "src/compat_stub.h"
#include "src/frame-private.h"
#include "src/icc-apply.h"
#include "src/icc-parse.h"
#include "src/loader.h"
#include "src/loader-manager.h"
#include "tests/data/color-numeric/tiff-icc.h"

static double
tiff_icc_scale(double const expected[3])
{
    double scale;
    int channel;

    scale = 0.0;
    for (channel = 0; channel < 3; ++channel) {
        if (fabs(expected[channel]) > scale) {
            scale = fabs(expected[channel]);
        }
    }
    return scale;
}

static int
tiff_icc_check_float(float const *actual, double const expected[][3],
                     size_t count)
{
    size_t pixel;
    size_t channel;
    double tolerance;

    for (pixel = 0; pixel < count; ++pixel) {
        tolerance = tiff_icc_scale(expected[pixel]) * tiff_icc_float_relative;
        for (channel = 0; channel < 3; ++channel) {
            if (!isfinite(actual[pixel * 3 + channel]) ||
                fabs(actual[pixel * 3 + channel] - expected[pixel][channel])
                    > tolerance) {
                fprintf(stderr, "TIFF ICC float pixel=%zu channel=%zu\n",
                    pixel, channel);
                return 0;
            }
        }
    }
    return 1;
}

static SIXELSTATUS
tiff_icc_capture_lab(sixel_frame_t *frame, void *context)
{
    float const *pixels;
    double expected[3];
    double tolerance;
    size_t pixel;
    size_t channel;
    int *calls;

    calls = context;
    ++*calls;
    if (frame->width != 16 || frame->height != 16 ||
        sixel_frame_get_pixelformat(frame) !=
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        frame->colorspace != SIXEL_COLORSPACE_LINEAR ||
        frame->transparent_mask != NULL) {
        return SIXEL_BAD_INPUT;
    }
    pixels = (float const *)sixel_frame_get_pixels(frame);
    for (pixel = 0; pixel < TIFF_ICC_LAB_COUNT; ++pixel) {
        /* This adapter still publishes bounded linear Lab output. The
         * signed endpoint checks above are a separate CMS contract. */
        for (channel = 0; channel < 3; ++channel) {
            expected[channel] = tiff_icc_lab_linear[pixel][channel];
            if (expected[channel] < 0.0) {
                expected[channel] = 0.0;
            } else if (expected[channel] > 1.0) {
                expected[channel] = 1.0;
            }
        }
        tolerance = tiff_icc_scale(expected) * tiff_icc_float_relative;
        for (channel = 0; channel < 3; ++channel) {
            if (!isfinite(pixels[pixel * 3 + channel]) ||
                fabs(pixels[pixel * 3 + channel] - expected[channel])
                    > tolerance) {
                fprintf(stderr, "TIFF Lab frame pixel=%zu channel=%zu\n",
                    pixel, channel);
                return SIXEL_BAD_INPUT;
            }
        }
    }
    return SIXEL_OK;
}

int
test_cms_0231(int argc, char **argv)
{
    sixel_icc_profile_t parsed;
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_transform_t *transform;
    sixel_cms_engine_t previous;
    sixel_loader_t *loader;
    SIXELSTATUS status;
    unsigned char *profile;
    void const *input;
    float *output;
    double const (*expected)[3];
    double xyz[3];
    double value;
    double tolerance;
    FILE *file;
    size_t pixel;
    size_t channel;
    size_t count;
    int profile_index;
    int mode;
    int calls;
    int no;
    int ok;

    if (argc != 3) {
        return 1;
    }
    mode = atoi(argv[1]);
    if (mode < 0 || mode > 7) {
        return 1;
    }
    if (mode == 7 && !loader_manager_entry_available("libtiff")) {
        return 77;
    }
    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    if (sixel_cms_get_engine() != SIXEL_CMS_ENGINE_BUILTIN) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    memset(&parsed, 0, sizeof(parsed));
    source = NULL;
    target = NULL;
    transform = NULL;
    loader = NULL;
    file = NULL;
    profile = NULL;
    output = NULL;
    no = 0;
    calls = 0;
    ok = 0;
    if (mode == 7) {
        status = sixel_loader_new(&loader, NULL);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT,
                                     &calls);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_loader_set_prefer_float32(loader, 1);
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_USE_PALETTE,
                                     &no);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
            "libtiff:cms_engine=builtin:cms_target=linear:prefer_8bit=0!");
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = sixel_loader_load_file(loader, argv[2], tiff_icc_capture_lab);
        ok = SIXEL_SUCCEEDED(status) && calls == 1;
        goto end;
    }

    /* The offline generator reads the actual IFD and freezes these two
     * small profile spans. Source/generator drift fails regeneration checks;
     * runtime evaluation always parses the original TIFF's embedded bytes. */
    profile_index = mode < 2 ? 0 : 1;
    profile = malloc(tiff_icc_sizes[profile_index]);
    file = sixel_compat_fopen(argv[2], "rb");
    if (profile == NULL || file == NULL ||
        fseek(file, tiff_icc_offsets[profile_index], SEEK_SET) != 0 ||
        fread(profile, 1, tiff_icc_sizes[profile_index], file) !=
            tiff_icc_sizes[profile_index]) {
        goto end;
    }
    if (mode == 0 || mode == 2) {
        if (!sixel_icc_parse_profile(profile, tiff_icc_sizes[profile_index],
                                      &parsed)) {
            goto end;
        }
        if (mode == 0) {
            for (pixel = 0; pixel < TIFF_ICC_CURVE_COUNT; ++pixel) {
                channel = (size_t)tiff_icc_curves[pixel].channel;
                if (!sixel_icc_parametric_device(&parsed.curves[channel],
                        tiff_icc_curves[pixel].input, &value)) {
                    goto end;
                }
                tolerance = fabs(tiff_icc_curves[pixel].expected)
                    * tiff_icc_double_relative;
                if (!isfinite(value) ||
                    fabs(value - tiff_icc_curves[pixel].expected)
                        > tolerance) {
                    fprintf(stderr, "TIFF ICC curve case=%zu\n", pixel);
                    goto end;
                }
            }
        } else {
            for (pixel = 0; pixel < TIFF_ICC_LAB_COUNT; ++pixel) {
                /* Independently encoded unit-domain inputs isolate LUT/PCS
                 * evaluation from the facade's normalized Lab adapter. */
                if (!sixel_icc_apply_device_to_xyz_d50_with_a2b_slot(xyz,
                        tiff_icc_lab_device[pixel], 3, &parsed, 0, 0)) {
                    goto end;
                }
                tolerance = tiff_icc_scale(tiff_icc_lab_pcs[pixel])
                    * tiff_icc_double_relative;
                for (channel = 0; channel < 3; ++channel) {
                    if (!isfinite(xyz[channel]) ||
                        fabs(xyz[channel] - tiff_icc_lab_pcs[pixel][channel])
                            > tolerance) {
                        fprintf(stderr, "TIFF ICC PCS pixel=%zu\n", pixel);
                        goto end;
                    }
                }
            }
        }
        ok = 1;
        goto end;
    }

    count = mode == 1 ? TIFF_ICC_RGB_COUNT : TIFF_ICC_LAB_COUNT;
    output = malloc(count * 3 * sizeof(float));
    source = sixel_cms_open_profile_from_mem(profile,
                                            tiff_icc_sizes[profile_index]);
    target = mode >= 5 ? sixel_cms_create_srgb_profile()
                      : sixel_cms_create_linear_srgb_profile();
    if (output == NULL || source == NULL || target == NULL) {
        goto end;
    }
    transform = sixel_cms_create_transform(source,
        mode == 1 ? SIXEL_CMS_PIXELFORMAT_RGB_8
                  : SIXEL_CMS_PIXELFORMAT_LAB_F32,
        target, SIXEL_CMS_PIXELFORMAT_RGB_F32,
        SIXEL_CMS_TRANSFORM_DEFAULT);
    if (transform == NULL) {
        goto end;
    }
    input = mode == 1 ? (void const *)tiff_icc_rgb_source
                     : (void const *)tiff_icc_lab_source;
    if (mode == 4 || mode == 6) {
        memcpy(output, input, count * 3 * sizeof(float));
        input = output;
    }
    expected = mode == 1 ? tiff_icc_rgb_linear
             : mode >= 5 ? tiff_icc_lab_gamma : tiff_icc_lab_linear;
    ok = sixel_cms_do_transform(transform, input, output, count) &&
        tiff_icc_check_float(output, expected, count);

end:
    if (file != NULL) {
        fclose(file);
    }
    sixel_loader_unref(loader);
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(target);
    sixel_cms_close_profile(source);
    sixel_icc_profile_destroy(&parsed);
    free(output);
    free(profile);
    sixel_cms_set_engine(previous);
    if (ok) {
        fprintf(stderr, "TIFFICC|mode=%d|verified\n", mode);
    }
    return ok ? 0 : 1;
}
