/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_TEST_PHASED_RENDEZVOUS_H
#define LIBSIXEL_TEST_PHASED_RENDEZVOUS_H
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include "src/encoder.h"
#include "src/gpu-program.h"
#include "tests/_support/planner/pipeline/playback_test_common.h"

#define AHEAD_CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "ahead line %d: %s\n", __LINE__, #x); \
    goto end; } } while (0)

#if SIXEL_ENABLE_THREADS
/* Gates observe lane ownership, never elapsed scheduling speed. */
struct phased_rendezvous_fixture {
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_playback_t *playback;
    struct sixel_encoder_solve_test_hooks hooks;
    sixel_playback_stage_sample_t samples[4];
    playback_test_buffer_t buffer;
    int initialized;
    int writer_entered, writer_open;
    int helper_entered, helper_open, helper_complete;
    int first_done, count, bad_sequence, gate_failed, fail_write;
    int started, bad_credit;
    int resolve_entered, resolve_open;
    int encode_entered, encode_open, second_resolved, second_encoded;
    int hold_resolve, hold_encode;
    sixel_playback_pipeline_sample_t pipeline_samples[4];
    uint64_t last_resolved;
    SIXELSTATUS prepared_status;
    SIXELSTATUS completed[4];
};
int phased_rendezvous_open(struct phased_rendezvous_fixture *s, char const *gpu,
                   char const *prepare, char const *delta);
void phased_rendezvous_close(struct phased_rendezvous_fixture *s);
int phased_rendezvous_submit(struct phased_rendezvous_fixture *s, int index);
int phased_rendezvous_open_portable(struct phased_rendezvous_fixture *s,
    char const *gpu);
int phased_rendezvous_open_rect(struct phased_rendezvous_fixture *s);
int phased_rendezvous_submit_origin(struct phased_rendezvous_fixture *s,
    int index, int origin);
int phased_rendezvous_wait(struct phased_rendezvous_fixture *s, int *value);
void phased_rendezvous_release(struct phased_rendezvous_fixture *s, int *gate);
#endif
#endif
