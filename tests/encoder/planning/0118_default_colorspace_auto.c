/* SPDX-License-Identifier: MIT */
#include "../../_support/processing/planner/colorspace_auto_common.h"

#include <6cells.h>
#include "src/cms.h"
#include "tests/_support/planner/pipeline/playback_test_common.h"
#include "tests/data/color-numeric/icc.h"

static int
auto_p3_pixels(void)
{
    float pixels[] = {
        .6875f, .5f, .25f, .35f, .6f, .8f,
        .35f, .6f, .8f, .6875f, .5f, .25f
    };
    /* Decode the Display P3 transfer, apply the D65 P3-to-sRGB matrix,
     * then encode the sRGB transfer. The two RGB percentage triplets are
     * (72.139,49.004,19.175) and (25.138,60.779,81.878). Round each
     * to a SIXEL percentage, then to the decoder's 0..255 range. */
    static unsigned char const expected[] = {
        184, 125, 48, 255, 64, 156, 209, 255,
        64, 156, 209, 255, 184, 125, 48, 255
    };
    playback_test_buffer_t stream = {NULL, 0, 0};
    sixel_encoder_t *encoder = NULL;
    sixel_output_t *output = NULL;
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    sixel_color_def_t *p3;
    unsigned char *decoded = NULL;
    int width, height;
    int result = EXIT_FAILURE;
    size_t i;
    SIXELSTATUS status;

    p3 = sixel_cdef_icc(color_icc, sizeof(color_icc));
    if (p3 == NULL || SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 2, 2,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) ||
        SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
        SIXEL_FAILED(sixel_output_new(&output, playback_test_write, &stream,
                                      NULL))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (SIXEL_FAILED(iface->vtbl->set_rgb_definition(iface, p3))) {
        goto end;
    }
    status = sixel_encoder_encode_frame(encoder, frame, output);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "Display P3 encode: %x %s\n", (unsigned)status,
                sixel_helper_get_additional_message());
        goto end;
    }
    if (SIXEL_FAILED(sixel_decode_direct(stream.bytes, (int)stream.size,
            &decoded, &width, &height, NULL)) || width != 2 || height != 2) {
        goto end;
    }
    for (i = 0; i < sizeof(expected); ++i) {
        if (decoded[i] != expected[i]) {
            fprintf(stderr, "pixel %u channel %u: got %u, expected %u\n",
                    (unsigned)(i / 4), (unsigned)(i % 4),
                    (unsigned)decoded[i], (unsigned)expected[i]);
            goto end;
        }
    }
    result = EXIT_SUCCESS;
end:
    free(stream.bytes);
    free(decoded);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_frame_unref(frame);
    sixel_cdef_free(p3);
    return result;
}

int
test_plan_0003_default_colorspace_auto(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "display-p3") == 0) {
        return auto_p3_pixels();
    }
    return colorspace_auto_check(0);
}
