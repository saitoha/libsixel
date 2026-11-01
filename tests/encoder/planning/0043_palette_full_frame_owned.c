/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "../../_support/planner/pipeline/palette_launch_test_common.h"

int
test_palette_scheduler_0043(int argc, char **argv)
{
    palette_store_fixture_t f;
    sixel_encoder_t *encoder = NULL;
    sixel_frame_t *source = NULL, *sample = NULL, *reference = NULL;
    sixel_palette_build_request_t *request = NULL;
    sixel_palette_frame_state_t state;
    sixel_palette_store_job_t *job;
    sixel_palette_selection_t result = {0};
    sixel_dither_t *expected = NULL, *actual = NULL;
    sixel_frame_pixels_view_t input, copy;
    sixel_frame_interface_t *source_if, *sample_if;
    sixel_palette_snapshot_profile_t const *profile;
    size_t size;
    int floating, failed;

    (void)argc;
    (void)argv;
    failed = 1;
    PALETTE_STORE_CHECK(!palette_store_fixture_init(&f));
    for (floating = 0; floating < 2; ++floating) {
        PALETTE_STORE_CHECK(!palette_launch_input(f.allocator, floating,
            SIXEL_PALETTE_SAMPLING_FULL_FRAME, &encoder, &source,
            &request, &sample));
        source_if = sixel_frame_as_interface(source);
        sample_if = sixel_frame_as_interface(sample);
        PALETTE_STORE_CHECK(source_if->vtbl->get_pixels(source_if, &input)
            == SIXEL_OK && sample_if->vtbl->get_pixels(sample_if, &copy)
            == SIXEL_OK);
        size = (size_t)768 * (floating ? sizeof(float) : 1);
        PALETTE_STORE_CHECK(input.width == copy.width &&
            input.height == copy.height &&
            input.pixelformat == copy.pixelformat &&
            input.colorspace == copy.colorspace && source != sample);
        PALETTE_STORE_CHECK(sixel_frame_get_pixels(source) !=
            sixel_frame_get_pixels(sample) &&
            !memcmp(sixel_frame_get_pixels(source),
                sixel_frame_get_pixels(sample), size));
        PALETTE_STORE_CHECK(palette_launch_clone(sample, f.allocator,
            &reference) == SIXEL_OK);
        PALETTE_STORE_CHECK(sixel_palette_build_request_execute(request,
            reference, &expected, &state, NULL, NULL) == SIXEL_OK);
        PALETTE_STORE_CHECK(sixel_palette_store_admit(f.store,
            (uint64_t)floating + 1, &request, &sample, NULL, &job) == SIXEL_OK);
        PALETTE_STORE_CHECK(sixel_palette_store_job_start(job, &f.scheduler)
            == SIXEL_OK);
        /* The source can change and die while the builder owns its copy. */
        memset(sixel_frame_get_pixels(source), 0, size);
        sixel_frame_unref(source);
        source = NULL;
        sixel_encoder_unref(encoder);
        encoder = NULL;
        PALETTE_STORE_CHECK(sixel_palette_store_job_join(job, SIXEL_OK)
            == SIXEL_OK);
        sixel_palette_store_job_result(job, &result);
        PALETTE_STORE_CHECK(sixel_palette_snapshot_clone(result.snapshot,
            &actual) == SIXEL_OK);
        profile = sixel_palette_snapshot_profile(result.snapshot);
        PALETTE_STORE_CHECK(profile->construction.sampling.effective ==
            SIXEL_PALETTE_SAMPLING_FULL_FRAME &&
            profile->construction.sampling.phase ==
                SIXEL_PALETTE_POLICY_EXECUTED &&
            profile->construction.sampling_source ==
                SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME);
        PALETTE_STORE_CHECK(palette_launch_equal(actual, expected, floating));
        sixel_palette_store_job_release(job);
        sixel_palette_store_reset(f.store, (uint64_t)floating + 1);
        sixel_palette_snapshot_unref(result.snapshot);
        result.snapshot = NULL;
        sixel_dither_unref(actual);
        actual = NULL;
        sixel_dither_unref(expected);
        expected = NULL;
        sixel_frame_unref(reference);
        reference = NULL;
    }
    failed = 0;
end:
    sixel_palette_snapshot_unref(result.snapshot);
    sixel_dither_unref(actual);
    sixel_dither_unref(expected);
    sixel_frame_unref(reference);
    sixel_frame_unref(source);
    sixel_frame_unref(sample);
    sixel_palette_build_request_free(request);
    sixel_encoder_unref(encoder);
    return palette_store_fixture_free(&f) || failed;
}
