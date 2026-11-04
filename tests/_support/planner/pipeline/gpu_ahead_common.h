/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_TEST_GPU_AHEAD_COMMON_H
#define LIBSIXEL_TEST_GPU_AHEAD_COMMON_H
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include "src/encoder.h"
#include "playback_test_common.h"

#define AHEAD_CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "ahead line %d: %s\n", __LINE__, #x); \
    goto end; } } while (0)

#if SIXEL_ENABLE_THREADS
/* Gates observe lane ownership, never elapsed scheduling speed. */
struct gpu_ahead_fixture {
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_playback_t *playback;
    struct sixel_encoder_prepare_test_hooks hooks;
    playback_test_buffer_t buffer;
    int initialized;
    int writer_entered, writer_open;
    int helper_entered, helper_open, helper_complete;
    int first_done, count, bad_sequence, gate_failed, fail_write;
    SIXELSTATUS prepared_status;
    SIXELSTATUS completed[4];
};
int gpu_ahead_open(struct gpu_ahead_fixture *s, char const *gpu,
                   char const *prepare, char const *delta);
void gpu_ahead_close(struct gpu_ahead_fixture *s);
int gpu_ahead_submit(struct gpu_ahead_fixture *s, int index);
int gpu_ahead_wait(struct gpu_ahead_fixture *s, int *value);
void gpu_ahead_release(struct gpu_ahead_fixture *s, int *gate);
int gpu_ahead_compare(char const *delta);
#endif
#endif
