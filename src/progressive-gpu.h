/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_PROGRESSIVE_GPU_H
#define LIBSIXEL_PROGRESSIVE_GPU_H

#include <stddef.h>
#include <stdint.h>
#include <sixel.h>

#define SIXEL_REPAIR_GPU_BATCH_MAX 262144U
#define SIXEL_REPAIR_GPU_SAMPLE_MAX 128U

typedef struct sixel_repair_gpu sixel_repair_gpu_t;

/* Immutable, canonical working-space triples, never RGB888 reinterpreted as
 * floats. Both planes cover the bound rectangle; absolute coordinates refer
 * to its origin in the retained surface. The owner changes the corresponding
 * revision whenever pixels or masks change. No device copy owns history. */
typedef struct sixel_repair_gpu_view {
    float const *source;
    float const *retained;
    unsigned char const *domain;
    unsigned char const *valid;
    size_t count;
    int width;
    int height;
    int origin_x;
    int origin_y;
    int plane_width;
    int plane_height;
    int workspace;
    uint64_t source_generation;
    uint64_t history_revision;
    uint64_t configuration_revision;
} sixel_repair_gpu_view_t;

typedef struct sixel_repair_gpu_request {
    sixel_repair_gpu_view_t view;
    int policy;
    size_t auto_threshold;
    size_t first;
    size_t count;
    /* A compact list is strictly increasing and uses rectangle-relative
     * indices. FIRST must be zero when this optional list is supplied. */
    uint64_t const *positions;
    double stop_squared;
    double selection_squared;
    unsigned int sample_limit;
    /* Optional batch-local eligibility filter, uploaded on every request.
     * PRESELECTED gathers a canonically verified mask without float tests.
     * Global strata use the complete selected population and the number of
     * selected pixels preceding this batch, never a per-batch partition. */
    unsigned char const *filter;
    int preselected;
    uint64_t sample_population;
    uint64_t sample_offset;
    unsigned int sample_total;
    unsigned int sample_first;
} sixel_repair_gpu_request_t;

typedef struct sixel_repair_gpu_sample {
    uint64_t position;
    uint64_t plane_position;
    float source[3];
    float error;
    uint64_t population;
} sixel_repair_gpu_sample_t;

/* These float reductions are proposals. In particular, zero eligible pixels
 * is never evidence of convergence: the CPU canonical audit is authoritative.
 * Partial batches are not whole-domain observations. No paint is committed. */
typedef struct sixel_repair_gpu_summary {
    uint32_t domain_count;
    uint32_t valid_count;
    uint32_t invalid_count;
    uint32_t eligible_count;
    uint32_t nonfinite;
    float maximum;
    float sum;
    uint32_t sample_count;
    uint64_t maximum_position;
} sixel_repair_gpu_summary_t;

typedef struct sixel_repair_gpu_result {
    sixel_repair_gpu_summary_t summary;
    sixel_repair_gpu_sample_t samples[SIXEL_REPAIR_GPU_SAMPLE_MAX];
    /* Logical array copies and buffer lengths, excluding driver overhead,
     * inline dispatch parameters and the caller-owned CPU source planes. */
    size_t uploaded_bytes;
    size_t downloaded_bytes;
    size_t resident_bytes;
} sixel_repair_gpu_result_t;

/* Queries already include positional noise in canonical working coordinates.
 * Palette entries are wire-projected, ordered triples. The caller may submit
 * a palette slice to resume even a one-distance work budget. Returned float
 * distances select proposals only; CPU acceptance uses the original target. */
typedef struct sixel_repair_gpu_lookup {
    float const *queries;
    float const *palette;
    size_t count;
    unsigned int colors;
    int policy;
} sixel_repair_gpu_lookup_t;

typedef struct sixel_repair_gpu_match {
    uint32_t index;
    float distance;
} sixel_repair_gpu_match_t;

/* Calls on one cache are serialized by its owner. A scan is synchronous and
 * bounded; cancellation/update checks belong between batches, never between
 * submission and completion. Eviction cannot change authoritative history. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_gpu_new(sixel_repair_gpu_t **cache,
    sixel_allocator_t *allocator);
SIXEL_INTERNAL_API void
sixel_repair_gpu_free(sixel_repair_gpu_t *cache);
SIXEL_INTERNAL_API void
sixel_repair_gpu_evict(sixel_repair_gpu_t *cache);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_gpu_scan(sixel_repair_gpu_t *cache,
    sixel_repair_gpu_request_t const *request,
    sixel_repair_gpu_result_t *result);
/* AUTO admission is decided for the complete selected population by the
 * owner, before splitting application into bounded batches. FALSE requests
 * CPU fallback; output matches remain untouched on every unsuccessful call. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_gpu_lookup(sixel_repair_gpu_t *cache,
    sixel_repair_gpu_lookup_t const *request,
    sixel_repair_gpu_match_t *matches);

#endif
