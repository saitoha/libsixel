/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#include "presentation.h"
#include "encoder.h"
#include "encoder-core.h"
#include "output.h"
#include "output-factory.h"
#include "compat_stub.h"
#include "tty.h"
#include "timer.h"
#include "sleep.h"

static double sixel_presentation_clock(sixel_presentation_t *presentation);
static int sixel_presentation_deadline_reached(void *priv);

/* A resource limit is a successful partial display, never a quality claim.
 * Reporting runs outside encoder ownership, once per bound source. */
static void
sixel_presentation_stop(sixel_presentation_t *presentation, int reason)
{
    char const *name;

    if (presentation->finished) {
        return;
    }
    presentation->finished = 1;
    presentation->stop_reason = reason;
    name = reason == SIXEL_PRESENTATION_STOP_CONVERGED ? "converged"
        : reason == SIXEL_PRESENTATION_STOP_STALLED ? "stalled"
        : reason == SIXEL_PRESENTATION_STOP_PASSES ? "max_passes"
        : "max_seconds";
    if (presentation->verbose) {
        fprintf(stderr, "progressive: stop=%s passes=%" PRIu64 "%s\n",
            name, presentation->passes,
            reason == SIXEL_PRESENTATION_STOP_PASSES ||
            reason == SIXEL_PRESENTATION_STOP_SECONDS
                ? " (quality not certified)" : "");
    }
}

/* Compare elapsed duration rather than adding to the clock: even the
 * largest finite user limit cannot overflow a computed absolute deadline. */
static SIXELSTATUS
sixel_presentation_check_limit(sixel_presentation_t *presentation)
{
    double now;

    if (presentation->finished || presentation->generation == 0) {
        return SIXEL_OK;
    }
    if (sixel_encoder_is_cancel_requested(presentation->encoder)) {
        return SIXEL_INTERRUPTED;
    }
    now = sixel_presentation_clock(presentation);
    if (!isfinite(now)) {
        return SIXEL_RUNTIME_ERROR;
    }
    if (presentation->max_passes &&
            presentation->passes >= presentation->max_passes) {
        sixel_presentation_stop(presentation, SIXEL_PRESENTATION_STOP_PASSES);
    } else if (presentation->max_seconds > 0 &&
            now - presentation->source_started >= presentation->max_seconds) {
        sixel_presentation_stop(presentation, SIXEL_PRESENTATION_STOP_SECONDS);
    }
    return SIXEL_OK;
}

static void
sixel_presentation_observe(sixel_presentation_t *presentation,
    sixel_progressive_stats_t const *stats)
{
    if (stats->outcome == SIXEL_PROGRESSIVE_FRAME_EMITTED) {
        ++presentation->passes;
    } else if (stats->outcome == SIXEL_PROGRESSIVE_CONVERGED) {
        sixel_presentation_stop(presentation,
            SIXEL_PRESENTATION_STOP_CONVERGED);
    } else if (stats->outcome == SIXEL_PROGRESSIVE_STALLED) {
        sixel_presentation_stop(presentation,
            SIXEL_PRESENTATION_STOP_STALLED);
    }
}

static double
sixel_presentation_now(void *priv)
{
    (void)priv;
    return sixel_timer_now();
}

static void
sixel_presentation_wait(unsigned int usec, void *priv)
{
    (void)priv;
    sixel_sleep(usec);
}

SIXELSTATUS
sixel_presentation_validate(sixel_encoder_t const *encoder)
{
    if (!encoder->progressive_enabled) {
        return SIXEL_OK;
    }
    if (encoder->color_option != SIXEL_COLOR_OPTION_DEFAULT ||
            encoder->penetrate_multiplexer || encoder->ormode ||
            encoder->fdrcs || encoder->fuse_macro ||
            encoder->macro_number >= 0 || encoder->output_is_png ||
            encoder->transparent_offset_left ||
            encoder->transparent_offset_top) {
        sixel_helper_set_additional_message(
            "progressive requires generated palettes and complete SIXEL "
            "output without macros, OR, offsets or multiplexer wrapping.");
        return SIXEL_BAD_ARGUMENT;
    }
    return SIXEL_OK;
}

/* Both cursor controls and image bytes use this one selected sink. Complete
 * host short writes before reporting acceptance to the retained-plane owner. */
static int
sixel_presentation_write_raw(char *data, int size, void *priv)
{
    sixel_presentation_t *presentation;
    size_t offset;
    ssize_t written;

    presentation = priv;
    if (presentation->destination != NULL) {
        return sixel_output_write_bytes(presentation->destination, data, size)
            == SIXEL_OK ? 0 : -1;
    }
    offset = 0;
    while (offset < (size_t)size) {
        written = sixel_compat_write(presentation->fd, data + offset,
                                     (size_t)size - offset);
#if HAVE_ERRNO_H
        if (written < 0 && errno == EINTR) {
            continue;
        }
#endif
        if (written <= 0 || (size_t)written > (size_t)size - offset) {
            return -1;
        }
        offset += (size_t)written;
    }
    return 0;
}

/* At most one complete repair is accepted ahead of the host sink. The
 * worker owns that storage until the full write finishes. Host acceptance
 * is measurable, but does not acknowledge terminal display completion. */
#if SIXEL_ENABLE_THREADS
static int
sixel_presentation_transport_worker(void *priv)
{
    sixel_presentation_t *presentation;
    size_t bytes;
    double start;
    double elapsed;
    int result;

    presentation = priv;
    for (;;) {
        sixel_mutex_lock(&presentation->transport_mutex);
        while (presentation->transport_bytes == 0 &&
                !presentation->transport_stop) {
            sixel_cond_wait(&presentation->transport_cond,
                &presentation->transport_mutex);
        }
        bytes = presentation->transport_bytes;
        if (bytes == 0 && presentation->transport_stop) {
            sixel_mutex_unlock(&presentation->transport_mutex);
            break;
        }
        sixel_mutex_unlock(&presentation->transport_mutex);
        start = presentation->now(presentation->clock_priv);
        result = sixel_presentation_write_raw(
            (char *)presentation->transport_buffer, (int)bytes, presentation);
        elapsed = presentation->now(presentation->clock_priv) - start;
        sixel_mutex_lock(&presentation->transport_mutex);
        if (result < 0) {
            presentation->transport_status = SIXEL_LIBC_ERROR;
            presentation->transport_stop = 1;
        } else if (elapsed > 0) {
            /* Retain the slowest observed rate for this playback session. */
            if (elapsed / bytes > presentation->seconds_per_byte) {
                presentation->seconds_per_byte = elapsed / bytes;
            }
        }
        presentation->transport_bytes = 0;
        sixel_cond_broadcast(&presentation->transport_cond);
        sixel_mutex_unlock(&presentation->transport_mutex);
    }
    return 0;
}

#endif

static SIXELSTATUS
sixel_presentation_drain(sixel_presentation_t *presentation)
{
    SIXELSTATUS status;

    if (!presentation->transport_initialized) {
        return SIXEL_OK;
    }
    sixel_mutex_lock(&presentation->transport_mutex);
    while (presentation->transport_bytes != 0) {
        sixel_cond_wait(&presentation->transport_cond,
            &presentation->transport_mutex);
    }
    status = presentation->transport_status;
    sixel_mutex_unlock(&presentation->transport_mutex);
    return status;
}

static int
sixel_presentation_write(char *data, int size, void *priv)
{
    sixel_presentation_t *presentation;
    char *prefix;
    size_t bytes;
    double start;
    double elapsed;
    int result;

    presentation = priv;
    if (size <= 0) {
        return size == 0 ? 0 : -1;
    }
    prefix = presentation->saved ? "\0338" : "\0337";
    if (presentation->async_submission) {
        bytes = (size_t)size + (presentation->prefix_pending ? 2u : 0u);
        sixel_mutex_lock(&presentation->transport_mutex);
        if (presentation->transport_bytes != 0 ||
                presentation->transport_stop ||
                bytes > presentation->transport_capacity) {
            sixel_mutex_unlock(&presentation->transport_mutex);
            return -1;
        }
        if (presentation->prefix_pending) {
            memcpy(presentation->transport_buffer, prefix, 2);
        }
        memcpy(presentation->transport_buffer + bytes - size, data, size);
        presentation->transport_bytes = bytes;
        presentation->saved = 1;
        presentation->prefix_pending = 0;
        sixel_cond_signal(&presentation->transport_cond);
        sixel_mutex_unlock(&presentation->transport_mutex);
        return 0;
    }
    start = presentation->now(presentation->clock_priv);
    if (presentation->prefix_pending) {
        if (sixel_presentation_write_raw(prefix, 2, presentation) < 0) {
            return -1;
        }
        presentation->saved = 1;
        presentation->prefix_pending = 0;
    }
    result = sixel_presentation_write_raw(data, size, presentation);
    elapsed = presentation->now(presentation->clock_priv) - start;
    /* Ordinary writes happen only after draining; the worker is idle. */
    if (elapsed > 0 &&
            elapsed / size > presentation->seconds_per_byte) {
        presentation->seconds_per_byte = elapsed / size;
    }
    return result;
}

SIXELSTATUS
sixel_presentation_init_owned(sixel_presentation_t *presentation,
    sixel_encoder_t *encoder, sixel_output_t *destination, int fd)
{
    sixel_progressive_config_t config;
    SIXELSTATUS status;

    memset(presentation, 0, sizeof(*presentation));
    presentation->encoder = encoder;
    presentation->now = sixel_presentation_now;
    presentation->wait = sixel_presentation_wait;
    presentation->quantum_seconds = 0.002;
    presentation->destination = destination;
    presentation->fd = fd;
    presentation->verbose = encoder->verbose;
    presentation->max_passes = encoder->progressive_max_passes;
    presentation->max_seconds = encoder->progressive_max_seconds;
    presentation->terminal = destination == NULL && sixel_compat_isatty(fd);
    status = sixel_presentation_validate(encoder);
    if (status != SIXEL_OK) {
        return status;
    }
    status = sixel_progressive_config_init(&config, sizeof(config));
    if (status == SIXEL_OK) {
        config.stop_distance_auto = encoder->progressive_stop_threshold < 0;
        config.stop_distance = config.stop_distance_auto ? 0
            : encoder->progressive_stop_threshold;
        status = sixel_repair_context_new_owned(&presentation->repair,
            encoder, &config, encoder->allocator);
    }
    if (status == SIXEL_OK) {
        /* Ownership is already held here. Only the file presentation path
         * selects this finite ladder; bare step clients keep their policy. */
        presentation->repair->ladder.allowance = 2;
        presentation->repair->yield_requested =
            sixel_presentation_deadline_reached;
        presentation->repair->yield_priv = presentation;
        status = sixel_encoder_core_create_output_from_factory(
            &presentation->output, sixel_presentation_write, presentation,
            encoder->allocator);
    }
    if (status != SIXEL_OK) {
        sixel_presentation_dispose_owned(presentation);
        return status;
    }
    sixel_encoder_core_require_keep_header(presentation->output, 1);
    presentation->budget.struct_size = sizeof(presentation->budget);
    presentation->budget.max_packet_bytes =
        SIXEL_PROGRESSIVE_DEFAULT_PACKET_BYTES;
    presentation->budget.max_pixel_visits =
        SIXEL_PROGRESSIVE_DEFAULT_PIXEL_VISITS;
    presentation->budget.max_distance_evaluations =
        SIXEL_PROGRESSIVE_DEFAULT_DISTANCE_EVALUATIONS;
#if SIXEL_ENABLE_THREADS
    if (encoder->update_policy == SIXEL_UPDATE_POLICY_DELTA &&
            !encoder->fstatic && !encoder->fignore_delay) {
        presentation->transport_capacity =
            SIXEL_PROGRESSIVE_DEFAULT_PACKET_BYTES + 2;
        presentation->transport_buffer = sixel_allocator_malloc(
            encoder->allocator, presentation->transport_capacity);
        if (presentation->transport_buffer == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        if (sixel_mutex_init(&presentation->transport_mutex) != 0) {
            return SIXEL_RUNTIME_ERROR;
        }
        if (sixel_cond_init(&presentation->transport_cond) != 0) {
            sixel_mutex_destroy(&presentation->transport_mutex);
            return SIXEL_RUNTIME_ERROR;
        }
        presentation->transport_initialized = 1;
        presentation->transport_status = SIXEL_OK;
        if (sixel_thread_create(&presentation->transport_thread,
                sixel_presentation_transport_worker, presentation) != 0) {
            return SIXEL_RUNTIME_ERROR;
        }
        presentation->transport_started = 1;
    }
#endif
    /* A new placement cannot inherit pixels from another file/session. */
    return sixel_encoder_invalidate_6delta_plane_owned(encoder);
}

void
sixel_presentation_dispose_owned(sixel_presentation_t *presentation)
{
    sixel_encoder_t *retained;

    if (presentation->transport_initialized) {
        sixel_cond_destroy(&presentation->transport_cond);
        sixel_mutex_destroy(&presentation->transport_mutex);
    }
    if (presentation->encoder != NULL) {
        sixel_allocator_free(presentation->encoder->allocator,
            presentation->transport_buffer);
    }
    retained = sixel_repair_context_detach_owned(presentation->repair);
    sixel_output_unref(presentation->output);
    sixel_encoder_unref(retained);
    memset(presentation, 0, sizeof(*presentation));
}


SIXELSTATUS
sixel_presentation_begin_image(sixel_presentation_t *presentation, int height)
{
    SIXELSTATUS status;

    if (!presentation->prepared && presentation->terminal) {
        status = sixel_tty_prepare_image_anchor(sixel_presentation_write_raw,
            presentation, presentation->fd, height);
        if (status != SIXEL_OK) {
            return status;
        }
    }
    presentation->prepared = 1;
    presentation->prefix_pending = 1;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_presentation_bind_owned(sixel_presentation_t *presentation,
                              sixel_frame_t *source)
{
    SIXELSTATUS status;
    double start;

    /* The ordinary image has just been accepted. Include bind/preprocess
     * time in this source's allowance, and publish only on successful bind. */
    start = sixel_presentation_clock(presentation);
    if (!isfinite(start)) {
        return SIXEL_RUNTIME_ERROR;
    }
    if (presentation->generation == UINT64_MAX) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    status = sixel_repair_context_bind_owned(presentation->repair, source,
        presentation->encoder->sixdelta_origin_x,
        presentation->encoder->sixdelta_origin_y,
        presentation->generation + 1);
    if (status == SIXEL_OK) {
        ++presentation->generation;
        presentation->finished = 0;
        presentation->stop_reason = SIXEL_PRESENTATION_STOP_NONE;
        presentation->source_started = start;
        presentation->passes = 1;
    }
    return status;
}

SIXELSTATUS
sixel_presentation_finish(sixel_presentation_t *presentation)
{
    sixel_progressive_stats_t stats;
    SIXELSTATUS status;

    if (presentation->repair == NULL || presentation->generation == 0) {
        return SIXEL_OK;
    }
    memset(&stats, 0, sizeof(stats));
    stats.struct_size = sizeof(stats);
    while (!presentation->finished) {
        status = sixel_presentation_check_limit(presentation);
        if (status != SIXEL_OK || presentation->finished) {
            return status;
        }
        presentation->prefix_pending = 1;
        status = sixel_progressive_step(presentation->repair,
            &presentation->budget, presentation->output, &stats);
        if (status != SIXEL_OK) {
            return status;
        }
        sixel_presentation_observe(presentation, &stats);
        switch (stats.outcome) {
        case SIXEL_PROGRESSIVE_FRAME_EMITTED:
            break;
        case SIXEL_PROGRESSIVE_YIELDED:
            if (stats.yield_reason != SIXEL_PROGRESSIVE_YIELD_WORK_BUDGET) {
                presentation->wait(1000, presentation->clock_priv);
            }
            break;
        case SIXEL_PROGRESSIVE_CONVERGED:
        case SIXEL_PROGRESSIVE_STALLED:
            break;
        case SIXEL_PROGRESSIVE_BUDGET_TOO_SMALL:
            if (stats.required_packet_bytes <=
                    presentation->budget.max_packet_bytes ||
                    stats.required_packet_bytes > INT_MAX) {
                return SIXEL_BAD_INTEGER_OVERFLOW;
            }
            presentation->budget.max_packet_bytes =
                stats.required_packet_bytes;
            break;
        default:
            sixel_helper_set_additional_message(
                "progressive presentation lost its source or baseline.");
            return SIXEL_RUNTIME_ERROR;
        }
    }
    return SIXEL_OK;
}

/* Keep playback moving on low-resolution or CPU-clock timer fallbacks.
 * Only the presentation thread owns this floor; transport measurements use
 * the raw clock. A sleep contributes its requested duration to the floor. */
static double
sixel_presentation_clock(sixel_presentation_t *presentation)
{
    double now;

    now = presentation->now(presentation->clock_priv);
    if (!isfinite(now)) {
        return now;
    }
    if (now > presentation->clock_floor) {
        presentation->clock_floor = now;
    }
    return presentation->clock_floor;
}

SIXELSTATUS
sixel_presentation_before_frame(sixel_presentation_t *presentation)
{
    SIXELSTATUS status;

    status = sixel_presentation_drain(presentation);
    if (status != SIXEL_OK) {
        return status;
    }
    if (sixel_encoder_is_cancel_requested(presentation->encoder)) {
        return SIXEL_INTERRUPTED;
    }
    if (!isfinite(sixel_presentation_clock(presentation))) {
        return SIXEL_RUNTIME_ERROR;
    }
    if (!presentation->clock_started) {
        presentation->deadline = sixel_presentation_clock(presentation);
        presentation->clock_started = 1;
    }
    return SIXEL_OK;
}

/* Called at bounded CPU checkpoints and again immediately before admission.
 * A later ordinary source cannot be made late by starting another packet
 * once the reserved computation/output margin has elapsed. An accepted host
 * write still has unbounded latency on a stalled blocking destination. */
static int
sixel_presentation_deadline_reached(void *priv)
{
    sixel_presentation_t *presentation;
    double now;

    presentation = priv;
    now = sixel_presentation_clock(presentation);
    return !isfinite(now) ||
        (presentation->timed && now >= presentation->admission_deadline) ||
        (presentation->max_seconds > 0 &&
         now - presentation->source_started >= presentation->max_seconds);
}

int
sixel_presentation_admission(double now, double deadline, double quantum,
    double seconds_per_byte, size_t pending, size_t capacity,
    size_t *packet_bytes, double *admission_deadline)
{
    double allowance;

    if (!isfinite(now) || !isfinite(deadline) || !isfinite(quantum) ||
            !isfinite(seconds_per_byte) || quantum <= 0 ||
            seconds_per_byte <= 0 || pending != 0 || capacity < 66) {
        return 0;
    }
    allowance = (deadline - now - 2 * quantum) / (2 * seconds_per_byte) - 2;
    if (allowance < 64) {
        return 0;
    }
    *packet_bytes = allowance >= capacity - 2 ? capacity - 2
        : (size_t)allowance;
    *admission_deadline = deadline -
        2 * seconds_per_byte * (*packet_bytes + 2);
    return *admission_deadline > now;
}

/* One independent repair image belongs to this source interval. Its budget
 * is reserved immediately after the ordinary image, not after an idle timer.
 * Neither a queued next source nor prefetch occupancy cancels this slot. */

SIXELSTATUS
sixel_presentation_after_frame(sixel_presentation_t *presentation,
                               int delay, int multiframe)
{
    sixel_encoder_t *encoder;
    sixel_progressive_budget_t budget;
    sixel_progressive_stats_t stats;
    SIXELSTATUS status;
    size_t pending;
    double now;
    double start;
    double elapsed;
    double slack;
    double rate;
    unsigned int sleep_usec;
    int attempted;
    int deferred;

    encoder = presentation->encoder;
    if (encoder->update_policy == SIXEL_UPDATE_POLICY_PROGRESSIVE ||
            encoder->fstatic || !multiframe) {
        return sixel_presentation_finish(presentation);
    }
    if (encoder->fignore_delay || delay <= 0) {
        return SIXEL_OK;
    }
    /* Only source durations advance this clock. Repair count and queue
     * prefetch never extend the next ordinary frame's absolute deadline. */
    presentation->deadline += delay / 100.0;
    budget = presentation->budget;
    budget.max_pixel_visits = 256;
    budget.max_distance_evaluations = 2048;
    memset(&stats, 0, sizeof(stats));
    stats.struct_size = sizeof(stats);
    status = SIXEL_OK;
    deferred = 0;
    presentation->repair->yield_requested =
        sixel_presentation_deadline_reached;
    presentation->repair->yield_priv = presentation;
    while (1) {
        if (sixel_encoder_is_cancel_requested(encoder)) {
            status = SIXEL_INTERRUPTED;
            break;
        }
        now = sixel_presentation_clock(presentation);
        if (!isfinite(now)) {
            status = SIXEL_RUNTIME_ERROR;
            break;
        }
        slack = presentation->deadline - now;
        if (slack <= 0) {
            break;
        }
        status = sixel_presentation_check_limit(presentation);
        if (status != SIXEL_OK) {
            break;
        }
        attempted = 0;
        /* No asynchronous capacity exists in a thread-disabled build.
         * Preserve normal animation, then refine its finite final frame. */
        if (presentation->transport_started && !presentation->finished &&
                !deferred) {
            sixel_mutex_lock(&presentation->transport_mutex);
            pending = presentation->transport_bytes;
            rate = presentation->seconds_per_byte;
            status = presentation->transport_status;
            sixel_mutex_unlock(&presentation->transport_mutex);
            if (status != SIXEL_OK) {
                break;
            }
            if (sixel_presentation_admission(now, presentation->deadline,
                presentation->quantum_seconds, rate, pending,
                presentation->transport_capacity,
                &budget.max_packet_bytes,
                &presentation->admission_deadline)) {
                presentation->timed = 1;
                presentation->async_submission = 1;
                presentation->prefix_pending = 1;
                start = sixel_presentation_clock(presentation);
                status = sixel_progressive_step(presentation->repair,
                    &budget, presentation->output, &stats);
                presentation->timed = 0;
                presentation->async_submission = 0;
                elapsed = sixel_presentation_clock(presentation)
                    - start;
                if (elapsed > presentation->quantum_seconds) {
                    presentation->quantum_seconds = elapsed;
                }
                if (status != SIXEL_OK) {
                    break;
                }
                sixel_presentation_observe(presentation, &stats);
                if (stats.outcome != SIXEL_PROGRESSIVE_CONVERGED &&
                        stats.outcome != SIXEL_PROGRESSIVE_STALLED &&
                        stats.outcome !=
                        SIXEL_PROGRESSIVE_FRAME_EMITTED &&
                        stats.outcome != SIXEL_PROGRESSIVE_YIELDED &&
                        stats.outcome != SIXEL_PROGRESSIVE_BUDGET_TOO_SMALL) {
                    status = SIXEL_RUNTIME_ERROR;
                    break;
                }
                if (stats.outcome == SIXEL_PROGRESSIVE_BUDGET_TOO_SMALL) {
                    /* Remaining capacity can only shrink this source period.
                     * Retry with the final-frame budget after playback. */
                    deferred = 1;
                }
                attempted = stats.outcome ==
                    SIXEL_PROGRESSIVE_FRAME_EMITTED ||
                    (stats.outcome == SIXEL_PROGRESSIVE_YIELDED &&
                     stats.yield_reason ==
                         SIXEL_PROGRESSIVE_YIELD_WORK_BUDGET);
            }
        }
        if (!attempted) {
            slack = presentation->deadline -
                sixel_presentation_clock(presentation);
            if (slack > 0) {
                sleep_usec = slack >= 0.001 ? 1000
                    : (unsigned int)(slack * 1000000) + 1;
                presentation->wait(sleep_usec, presentation->clock_priv);
                presentation->clock_floor += sleep_usec / 1000000.0;
            }
        }
    }
    presentation->timed = 0;
    presentation->async_submission = 0;
    return status;
}

SIXELSTATUS
sixel_presentation_shutdown(sixel_presentation_t *presentation)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (presentation->transport_started) {
        status = sixel_presentation_drain(presentation);
        sixel_mutex_lock(&presentation->transport_mutex);
        presentation->transport_stop = 1;
        sixel_cond_signal(&presentation->transport_cond);
        sixel_mutex_unlock(&presentation->transport_mutex);
        sixel_thread_join(&presentation->transport_thread);
        presentation->transport_started = 0;
    }
    return status;
}
