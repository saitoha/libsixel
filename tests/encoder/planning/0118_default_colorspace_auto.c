/* SPDX-License-Identifier: MIT */
#include "../../_support/processing/planner/colorspace_auto_common.h"

#include <6cells.h>
#include "tests/_support/planner/pipeline/playback_test_common.h"
#include "tests/data/color-numeric/icc.h"

enum p3_case {
    P3_AUTO, P3_GAMMA, P3_GAMMA_PLAYBACK, P3_GAMMA_RESIZE
};

static SIXELSTATUS
init_p3(sixel_frame_t **frame, float *pixels, int height,
        sixel_color_def_t const *p3)
{
    SIXELSTATUS status;

    status = sixel_frame_new(frame, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_frame_init_rgbf32_borrowed(*frame, pixels, 2, height,
                                                p3);
    }
    return status;
}

static int
check_p3_pixels(enum p3_case mode)
{
    float auto_pixels[] = {
        .6875f, .5f, .25f, .35f, .6f, .8f,
        .35f, .6f, .8f, .6875f, .5f, .25f
    };
    /* Decode the Display P3 transfer, apply the D65 P3-to-sRGB matrix,
     * then encode the sRGB transfer. The two RGB percentage triplets are
     * (72.139,49.004,19.175) and (25.138,60.779,81.878). Round each
     * to a SIXEL percentage, then to the decoder's 0..255 range. */
    static unsigned char const auto_expected[] = {
        184, 125, 48, 255, 64, 156, 209, 255,
        64, 156, 209, 255, 184, 125, 48, 255
    };
    /* The close P3 colors below encode to sRGB percentages
     * (52.314,36.857,22.811) and (49.914,37.506,25.241) using the same
     * independent D65 calculation. Both colors fit in the palette.
     * Nearest-neighbor height doubling repeats this row exactly. */
    float gamma_pixels[] = {.5f, .375f, .25f, .48f, .38f, .27f};
    static unsigned char const gamma_expected[] = {
        133, 94, 59, 255, 128, 97, 64, 255
    };
    /* Slightly offset palette colors prove playback sampled the separate
     * frame. Its percentages are (52.892,36.826,22.786) and
     * (49.914,37.506,26.422), with the same nearest entry per pixel. */
    float palette_pixels[] = {.505f, .375f, .25f, .48f, .38f, .28f};
    static unsigned char const playback_expected[] = {
        135, 94, 59, 255, 128, 97, 66, 255
    };
    unsigned char const *expected;
    unsigned char expected_byte;
    float *pixels;
    int input_height;
    int output_height;
    size_t expected_size;
    sixel_frame_t *source = NULL;
    sixel_playback_t *session = NULL;
    sixel_playback_options_t options;
    playback_test_buffer_t stream = {NULL, 0, 0};
    sixel_encoder_t *encoder = NULL;
    sixel_output_t *output = NULL;
    sixel_frame_t *frame = NULL;
    sixel_color_def_t *p3;
    unsigned char *decoded = NULL;
    int width, height;
    int result = EXIT_FAILURE;
    size_t i;
    SIXELSTATUS status;

    pixels = mode == P3_AUTO ? auto_pixels : gamma_pixels;
    input_height = mode == P3_AUTO ? 2 : 1;
    output_height = mode == P3_GAMMA_RESIZE ? 2 : input_height;
    expected = mode == P3_AUTO ? auto_expected : gamma_expected;
    expected_size = mode == P3_AUTO ? sizeof(auto_expected) :
                                      sizeof(gamma_expected);
    p3 = sixel_color_def_new_icc(color_icc, sizeof(color_icc));
    if (p3 == NULL || SIXEL_FAILED(init_p3(&frame, pixels, input_height, p3)) ||
        SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
        SIXEL_FAILED(sixel_output_new(&output, playback_test_write, &stream,
                                      NULL))) {
        goto end;
    }
    if (mode != P3_AUTO &&
        (SIXEL_FAILED(sixel_encoder_setopt(encoder,
             SIXEL_OPTFLAG_WORKING_COLORSPACE, "gamma")) ||
         SIXEL_FAILED(sixel_encoder_setopt(encoder,
             SIXEL_OPTFLAG_DIFFUSION, "none")) ||
         SIXEL_FAILED(sixel_encoder_setopt(encoder,
             SIXEL_OPTFLAG_THREADING_POLICY, "greedy:B1")) ||
         SIXEL_FAILED(sixel_encoder_setopt(encoder,
             SIXEL_OPTFLAG_GPU_POLICY, "off")))) {
        goto end;
    }
    if (mode == P3_GAMMA_RESIZE &&
        (SIXEL_FAILED(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_WIDTH,
                                          "2")) ||
         SIXEL_FAILED(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_HEIGHT,
                                          "2")) ||
         SIXEL_FAILED(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_RESAMPLING,
                                          "nearest")) ||
         SIXEL_FAILED(sixel_encoder_setopt(encoder,
             SIXEL_OPTFLAG_RUNTIME_POLICY,
             "scalar:resize_precision=preserve")))) {
        goto end;
    }
    if (mode == P3_GAMMA_PLAYBACK) {
        if (SIXEL_FAILED(init_p3(&source, palette_pixels, 1, p3)) ||
            SIXEL_FAILED(sixel_playback_options_init(&options,
                sizeof(options))) ||
            SIXEL_FAILED(sixel_playback_new(&session, encoder, output,
                NULL, NULL))) {
            goto end;
        }
        options.palette_source = source;
        expected = playback_expected;
        status = sixel_playback_submit(session, frame, SIXEL_PLAYBACK_WAIT,
                                       -1, &options);
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_playback_drain(session, -1);
        }
    } else {
        status = sixel_encoder_encode_frame(encoder, frame, output);
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "Display P3 encode: %x %s\n", (unsigned)status,
                sixel_helper_get_additional_message());
        goto end;
    }
    if (SIXEL_FAILED(sixel_decode_direct(stream.bytes, (int)stream.size,
            &decoded, &width, &height, NULL)) ||
        width != 2 || height != output_height) {
        goto end;
    }
    for (i = 0; i < (size_t)height * 8; ++i) {
        expected_byte = expected[i % expected_size];
        if (decoded[i] != expected_byte) {
            fprintf(stderr, "pixel %u channel %u: got %u, expected %u\n",
                    (unsigned)(i / 4), (unsigned)(i % 4),
                    (unsigned)decoded[i], (unsigned)expected_byte);
            goto end;
        }
    }
    result = EXIT_SUCCESS;
end:
    sixel_playback_unref(session);
    sixel_frame_unref(source);
    free(stream.bytes);
    free(decoded);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_frame_unref(frame);
    sixel_color_def_free(p3);
    return result;
}

int
test_plan_0003_default_colorspace_auto(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "display-p3") == 0) {
        return check_p3_pixels(P3_AUTO);
    }
    if (argc == 2 && strcmp(argv[1], "gamma-p3") == 0) {
        return check_p3_pixels(P3_GAMMA);
    }
    if (argc == 2 && strcmp(argv[1], "gamma-p3-playback") == 0) {
        return check_p3_pixels(P3_GAMMA_PLAYBACK);
    }
    if (argc == 2 && strcmp(argv[1], "gamma-p3-resize") == 0) {
        return check_p3_pixels(P3_GAMMA_RESIZE);
    }
    return colorspace_auto_check(0);
}
