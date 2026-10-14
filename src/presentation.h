/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_PRESENTATION_H
#define LIBSIXEL_PRESENTATION_H

#include "progressive.h"
#include "threading.h"

#define SIXEL_PRESENTATION_DEFAULT_MAX_SECONDS 5.0

enum {
    SIXEL_PRESENTATION_STOP_NONE,
    SIXEL_PRESENTATION_STOP_CONVERGED,
    SIXEL_PRESENTATION_STOP_STALLED,
    SIXEL_PRESENTATION_STOP_PASSES,
    SIXEL_PRESENTATION_STOP_SECONDS
};

typedef struct sixel_presentation {
    sixel_encoder_t *encoder;
    sixel_repair_context_t *repair;
    sixel_output_t *output;
    sixel_output_t *destination;
    int fd;
    int prepared;
    int saved;
    int prefix_pending;
    int terminal;
    int finished;
    int stop_reason;
    int verbose;
    unsigned int max_passes;
    double max_seconds;
    double source_started;
    uint64_t passes;
    /* Clock/sink observations are private and injectable for scheduler tests.
     * Queue occupancy describes accepted output, never prefetched sources. */
    double (*now)(void *priv);
    void (*wait)(unsigned int usec, void *priv);
    void *clock_priv;
    double deadline;
    double clock_floor;
    double admission_deadline;
    double quantum_seconds;
    double seconds_per_byte;
    int clock_started;
    int timed;
    int async_submission;
    sixel_mutex_t transport_mutex;
    sixel_cond_t transport_cond;
    sixel_thread_t transport_thread;
    unsigned char *transport_buffer;
    size_t transport_bytes;
    size_t transport_capacity;
    int transport_initialized;
    int transport_started;
    int transport_stop;
    SIXELSTATUS transport_status;
    uint64_t generation;
    sixel_progressive_budget_t budget;
} sixel_presentation_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_presentation_validate(sixel_encoder_t const *encoder);
/* Initialization, binding and disposal require encoder ownership. The
 * caller keeps the encoder alive independently of its repair context. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_presentation_init_owned(sixel_presentation_t *presentation,
    sixel_encoder_t *encoder, sixel_output_t *destination, int fd);
SIXEL_INTERNAL_API void
sixel_presentation_dispose_owned(sixel_presentation_t *presentation);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_presentation_begin_image(sixel_presentation_t *presentation,
                               int height);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_presentation_bind_owned(sixel_presentation_t *presentation,
                              sixel_frame_t *source);
/* Pure admission policy over measured transport and clock observations. */
SIXEL_INTERNAL_API int
sixel_presentation_admission(double now, double deadline, double quantum,
    double seconds_per_byte, size_t pending, size_t capacity,
    size_t *packet_bytes, double *admission_deadline);
/* All waits, queue draining and shutdown run without encoder ownership. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_presentation_before_frame(sixel_presentation_t *presentation);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_presentation_after_frame(sixel_presentation_t *presentation,
                               int delay, int multiframe);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_presentation_shutdown(sixel_presentation_t *presentation);
/* Advance only after releasing image ownership; step acquires its own gate. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_presentation_finish(sixel_presentation_t *presentation);

#endif
