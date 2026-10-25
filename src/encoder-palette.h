/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_ENCODER_PALETTE_H
#define LIBSIXEL_ENCODER_PALETTE_H

#include <sixel.h>
#include "palette-plan.h"
#include "timeline-logger.h"

/* Construction owns every referenced endpoint. Call new under the encoder's
 * operation ownership, before publishing the request to any worker. The
 * caller supplies a private sample to build; it may be converted in place.
 * Consumer format comes from the frame-owned plan, including late retry. */
typedef struct sixel_palette_build_request sixel_palette_build_request_t;
typedef struct sixel_palette_snapshot sixel_palette_snapshot_t;

typedef struct sixel_palette_snapshot_profile {
    int working_colorspace;
    int clustering_colorspace;
    int output_colorspace;
    int output_color_definition;
    int consumer_pixelformat;
    int precision_requested;
    int delta_alpha_key;
    int source_alpha_key;
    uint64_t configuration_revision;
    struct sixel_color_def const *output_icc;
    int keycolor;
    int keycolor_reserved;
    int paintable_colors;
    sixel_palette_frame_state_t construction;
} sixel_palette_snapshot_profile_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_build_request_new(sixel_palette_build_request_t **out,
    sixel_encoder_t const *encoder, sixel_frame_t *frame,
    sixel_palette_frame_state_t const *state, int from_map_image,
    int consumer_pixelformat);
/* Prepare at the sampling policy's original boundary: adaptive-grid before
 * preplan, full-frame after preplan. Both return an owned input; full-frame
 * clones the preprocessed pixels, alpha and metadata before launching.
 * Allocation failure leaves the source and state intact for frame fallback. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_build_input_new(sixel_palette_build_request_t **request,
    sixel_frame_t **sample, sixel_encoder_t *encoder, sixel_frame_t *frame,
    sixel_palette_frame_state_t const *state, int consumer_pixelformat);
SIXEL_INTERNAL_API void
sixel_palette_build_request_free(sixel_palette_build_request_t *request);

/* Build only; no cache, capture, history or live planner access. The result
 * state is caller-owned and must be collected after the worker has joined.
 * Optional retryable reports construction failure, excluding lookup errors. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_build_request_execute(
    sixel_palette_build_request_t const *request, sixel_frame_t *sample,
    sixel_dither_t **out, sixel_palette_frame_state_t *state,
    sixel_timeline_logger_t *logger, int *retryable);

/* Transfer the completed builder dither on success (clear *built). Neither
 * the caller nor an application may mutate the transferred object. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_snapshot_take(sixel_palette_snapshot_t **out,
    sixel_palette_build_request_t const *request, sixel_dither_t **built,
    sixel_palette_frame_state_t const *state);
SIXEL_INTERNAL_API void
sixel_palette_snapshot_ref(sixel_palette_snapshot_t *snapshot);
SIXEL_INTERNAL_API void
sixel_palette_snapshot_unref(sixel_palette_snapshot_t *snapshot);
SIXEL_INTERNAL_API sixel_palette_snapshot_profile_t const *
sixel_palette_snapshot_profile(sixel_palette_snapshot_t const *snapshot);

/* Copy entries exactly, with fresh application/scratch state and a separate
 * lookup wrapper. Only FHEDT may retain a compatible immutable table. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_snapshot_clone(sixel_palette_snapshot_t const *snapshot,
    sixel_dither_t **out);

#endif /* LIBSIXEL_ENCODER_PALETTE_H */
