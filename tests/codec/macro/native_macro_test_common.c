/* SPDX-License-Identifier: MIT */
/* Exercise the actual fd/hex writer with typed, borrowed source pixels. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"

int
macro_native_contract(int scenario)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "balanced:B1" }, { 'p', "1" }, { 'Q', "heckbert" },
        { 'F', "none" }, { 'a', "off" }, { '_', "none" },
        { '~', "none" }, { 'd', "none" }, { 'E', "fast" },
        { 'X', "linear" }, { 'W', "linear" }, { '7', NULL },
        { '4', "full-frame" }, { '5', "none" }, { 'g', NULL }
    };
    float pixels[9] = { 2.f, .1f, .1f, -.25f, .1f, .1f, 9.f, 9.f, 9.f };
    unsigned char bytes[6] = { 64, 64, 64, 192, 192, 192 };
    float saved[9];
    double expected[3] = { .875, .1, .1 };
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    sixel_frame_interface_t *iface;
    sixel_frame_transparency_t transparency;
    unsigned char *mask;
    size_t n;
    int step;
    int steps;
    int byte;
    int p3;
    int ok;
    SIXELSTATUS status;

    encoder = NULL;
    frame = NULL;
    mask = NULL;
    ok = 0;
    byte = scenario == 6;
    steps = scenario == 5 ? 4 : scenario == 7 ? 3 : 1;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL))) {
        goto end;
    }
    for (n = 0; n < sizeof(options) / sizeof(options[0]); ++n) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[n].flag,
                options[n].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder,
            scenario == 2 || scenario == 5 ? 'u' : 'n',
            scenario == 2 || scenario == 5 ? NULL : "7"))) {
        goto end;
    }
    if (scenario == 3 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
            '8', NULL))) {
        goto end;
    }
    if (byte && (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X', "gamma")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', "gamma")))) {
        goto end;
    }
    if (scenario == 4 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
            'p', "2"))) {
        goto end;
    }
    for (step = 0; step < steps; ++step) {
        p3 = scenario != 0 && scenario != 5 && !byte &&
            !(scenario == 7 && step == 1);
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                p3 ? "display-p3" : "gamma"))) {
            goto end;
        }
        if (scenario == 5) {
            /* The second frame has a different independently known gray.
             * The next loop must invoke IDs, not define these frames again. */
            for (n = 0; n < 6; ++n) {
                pixels[n] = step % 2 ? .11f : .1f;
            }
            for (n = 0; n < 3; ++n) {
                expected[n] = step % 2 ? .11 : .1;
            }
        }
        memcpy(saved, pixels, sizeof(saved));
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    byte ? (void *)bytes : (void *)pixels,
                    scenario == 4 ? 3 : 2, 1, byte ?
                    SIXEL_PIXELFORMAT_RGB888 :
                    SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1))) {
            goto end;
        }
        if (scenario == 4) {
            mask = calloc(3, 1);
            if (mask == NULL) {
                goto end;
            }
            mask[2] = 1;
            memset(&transparency, 0, sizeof(transparency));
            transparency.transparent = -1;
            transparency.alpha_zero_is_transparent = 1;
            transparency.transparent_mask = mask;
            transparency.transparent_mask_size = 3;
            iface = sixel_frame_as_interface(frame);
            if (SIXEL_FAILED(iface->vtbl->set_transparency(iface,
                    &transparency))) {
                goto end;
            }
            mask = NULL;
        }
        if (scenario == 5) {
            sixel_frame_set_multiframe(frame, 1);
            sixel_frame_set_frame_no(frame, step % 2);
            sixel_frame_set_loop_count(frame, step / 2);
        }
        /* NULL output selects the real fd-backed macro/hex callback. A
         * supplied raw callback would not test that transport boundary. */
        status = sixel_encoder_encode_frame(encoder, frame, NULL);
        if (SIXEL_FAILED(status)) {
            fprintf(stderr, "macro status=%d: %s\n", status,
                sixel_helper_get_additional_message());
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (dither == NULL || dither->use_rgb101_palette != !byte ||
                memcmp(pixels, saved, sizeof(saved)) != 0) {
            goto end;
        }
        if (!byte) {
            if (SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                    dither->palette, &view)) || view.entries == NULL ||
                    view.entry_count != (scenario == 4 ? 2U : 1U)) {
                goto end;
            }
            /* A few float storage ULPs are allowed. Byte reconstruction,
             * source clamping and replacing centers with wire codes fail. */
            for (n = 0; n < 3; ++n) {
                if (!isfinite(view.entries[n]) ||
                        fabs(view.entries[n] - expected[n]) > 5e-7) {
                    fprintf(stderr, "macro center[%lu]=%.9g != %.9g\n",
                        (unsigned long)n, view.entries[n], expected[n]);
                    goto end;
                }
            }
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    free(mask);
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
