/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/rendezvous.h"
#include <stdlib.h>
#include "src/dither.h"

int
test_dither_fs_gpu_0015(int argc, char **argv)
{
#if SIXEL_ENABLE_THREADS
    static char const *const policies[] = { "off", "auto", "force" };
    static char const *const colors[] = { "16", "64" };
    static int const methods[] = { SIXEL_DIFFUSE_ATKINSON, SIXEL_DIFFUSE_FS };
    playback_test_buffer_t buffer = { 0 };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t const *dither;
    sixel_gpu_statistics_t before, after;
    int ok, device;
    size_t i, j;

    (void)argc;
    (void)argv;
    device = lso2_gpu_rendezvous_device();
    if (device != 0) return device;
    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    AHEAD_CHECK(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, '.', "8bit") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, 'W', "gamma") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, 'd', "auto") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, '~', "auto") == SIXEL_OK);
    AHEAD_CHECK(sixel_output_new(&output, playback_test_write, &buffer, NULL)
        == SIXEL_OK);
    frame = playback_test_small_frame(0);
    AHEAD_CHECK(frame != NULL);
    for (i = 0; i < sizeof(colors) / sizeof(colors[0]); ++i) {
        AHEAD_CHECK(sixel_encoder_setopt(encoder, 'p', colors[i]) == SIXEL_OK);
        for (j = 0; j < sizeof(policies) / sizeof(policies[0]); ++j) {
            AHEAD_CHECK(sixel_encoder_setopt(encoder, 'G', policies[j])
                == SIXEL_OK);
            AHEAD_CHECK(sixel_gpu_get_statistics(&before) == SIXEL_OK);
            AHEAD_CHECK(sixel_encoder_encode_frame(encoder, frame, output)
                == SIXEL_OK);
            AHEAD_CHECK(sixel_gpu_get_statistics(&after) == SIXEL_OK);
            dither = encoder->diagnostic_dither;
            AHEAD_CHECK(dither != NULL && dither->apply_success_count > 0);
            AHEAD_CHECK(dither->method_for_diffuse == methods[i]);
            AHEAD_CHECK(dither->diffuse_on_gpu == (j != 0));
            AHEAD_CHECK((after.dispatches > before.dispatches) == (j != 0));
        }
    }
    /* The GPU FHEDT preparer declines its CPU lookup cache option. */
    AHEAD_CHECK(sixel_encoder_setopt(encoder, 'G', "auto") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, '~',
        "fhedt:resolution=64:cache=1") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_encode_frame(encoder, frame, output) == SIXEL_OK);
    dither = encoder->diagnostic_dither;
    AHEAD_CHECK(dither != NULL && dither->apply_success_count > 0);
    AHEAD_CHECK(dither->method_for_diffuse == SIXEL_DIFFUSE_FS &&
        !dither->diffuse_on_gpu);
    AHEAD_CHECK(sixel_encoder_setopt(encoder, 'G', "force") == SIXEL_OK);
    AHEAD_CHECK(sixel_encoder_encode_frame(encoder, frame, output)
        == SIXEL_FEATURE_ERROR);
    ok = 1;
end:
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    free(buffer.bytes);
    return !ok;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
