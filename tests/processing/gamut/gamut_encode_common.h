/* SPDX-License-Identifier: MIT */
#include "gamut_test_common.h"
#include "tests/data/color-numeric/edge-reference.h"

typedef struct gamut_capture {
    unsigned char bytes[8192];
    size_t size;
} gamut_capture_t;

static inline int
gamut_capture_write(char *bytes, int size, void *context)
{
    gamut_capture_t *capture;

    capture = context;
    if (size < 0 || (size_t)size > sizeof(capture->bytes) - capture->size) {
        return -1;
    }
    memcpy(capture->bytes + capture->size, bytes, (size_t)size);
    capture->size += (size_t)size;
    return size;
}

/* Three source colors form an unambiguous two-cluster linear partition.
 * Its red centroid must be the mean of mapped samples, not a mapped mean
 * of out-of-gamut samples. This observes mapping BEFORE construction. */
static inline int
gamut_encode_check(char const *sampling, char const *working, int highcolor,
                   int edge)
{
    sixel_encoder_t *encoder = NULL;
    sixel_output_t *output = NULL;
    sixel_frame_t *frame = NULL;
    sixel_palette_float32_entries_view_t view;
    sixel_dither_t *dither;
    gamut_capture_t capture = {{0}, 0};
    float pixels[9] = {1.4f, -.1f, .15f, 1.1f, -.02f, .05f, 0, 0, 0};
    float saved[9];
    double expected[3];
    double converted[3];
    double entry[3];
    unsigned char *decoded = NULL;
    int width;
    int height;
    int ok;
    int found;
    int c;
    unsigned int i;

    memcpy(saved, pixels, sizeof(saved));
    ok = 0;
    found = 0;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, gamut_capture_write,
                &capture, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                edge ? "gamma:gamut=edge-seeker"
                    : "gamma:gamut=local-minde")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', "kmeans:seed=1")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X', "linear")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', working)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'a', "off")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'F', "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'd', "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_SAMPLING_POLICY, sampling)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder,
                highcolor ? 'I' : 'p', highcolor ? NULL : "2")) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 3, 1,
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1)) ||
            SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output)) ||
            memcmp(pixels, saved, sizeof(saved)) != 0 ||
            SIXEL_FAILED(sixel_decode_direct(capture.bytes, (int)capture.size,
                &decoded, &width, &height, NULL)) ||
            width != 3 || height != 1) {
        goto end;
    }
    if (highcolor) {
        /* RGB101 bins are independently derived from the reference. The
         * local MINDE gives (100,9,46), EdgeSeeker (100,22,47). */
        ok = decoded[0] == 255 && decoded[1] == (edge ? 56 : 23) &&
            decoded[2] == (edge ? 120 : 117);
        goto end;
    }
    dither = encoder->diagnostic_dither;
    if (dither == NULL || SIXEL_FAILED(dither->palette->vtbl
            ->get_entries_float32(dither->palette, &view))) {
        goto end;
    }
    for (c = 0; c < 3; ++c) {
        expected[c] = edge
            ? (edge_expected[0][0][c] + edge_expected[0][4][c]) * .5
            : (gamut_expected[0][0][c] + gamut_expected[0][4][c]) * .5;
    }
    for (i = 0; i < view.entry_count; ++i) {
        for (c = 0; c < 3; ++c) {
            entry[c] = view.entries[i * 3u + (unsigned int)c];
        }
        if (!sixel_color_convert(converted, entry,
                strcmp(working, "linear") == 0 ? SIXEL_COLORSPACE_LINEAR
                    : SIXEL_COLORSPACE_OKLAB, SIXEL_COLORSPACE_LINEAR)) {
            goto end;
        }
        if (converted[0] > .5) {
            found = 1;
            for (c = 0; c < 3; ++c) {
                if (!isfinite(converted[c]) ||
                        fabs(converted[c] - expected[c]) > 4e-5) {
                    fprintf(stderr, "mapped centroid[%d] %.12g != %.12g\n",
                        c, converted[c], expected[c]);
                    goto end;
                }
            }
        }
    }
    ok = found;
end:
    if (!ok) {
        fprintf(stderr, "gamut encode: %s\n",
            sixel_helper_get_additional_message());
    }
    free(decoded);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
