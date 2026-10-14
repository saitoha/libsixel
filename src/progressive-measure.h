/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_PROGRESSIVE_MEASURE_H
#define LIBSIXEL_PROGRESSIVE_MEASURE_H

#include <sixel.h>
#include <stdint.h>
#include "progressive-palette.h"
#include "progressive-predict.h"
#include "progressive-ladder.h"

#define SIXEL_REPAIR_MEASURE_BANDS SIXEL_REPAIR_PREDICT_BANDS
#define SIXEL_REPAIR_MEASURE_SAMPLES 256

enum sixel_repair_measure_phase {
    SIXEL_REPAIR_MEASURE_AUDIT,
    SIXEL_REPAIR_MEASURE_SELECT,
    SIXEL_REPAIR_MEASURE_PALETTE,
    SIXEL_REPAIR_MEASURE_APPLY,
    SIXEL_REPAIR_MEASURE_RESCUE,
    SIXEL_REPAIR_MEASURE_PACKET,
    SIXEL_REPAIR_MEASURE_WRITE,
    SIXEL_REPAIR_MEASURE_COMMIT,
    SIXEL_REPAIR_MEASURE_PHASES
};

typedef struct sixel_repair_measure_cost {
    uint64_t visits;
    uint64_t distances;
    double seconds;
} sixel_repair_measure_cost_t;

/* A systematic position sample, not a quality certificate or a weighted
 * histogram. Keep target color paired with its error: error magnitude alone
 * cannot predict the benefit of a particular palette. Positions missing from
 * the sample can be transparent or lack valid retained history. */
typedef struct sixel_repair_measure_sample {
    size_t position;
    float target[3];
    double error;
} sixel_repair_measure_sample_t;

/* Private observation ABI. Errors and band boundaries are squared distances
 * in the binding's working space. Band zero includes the stop boundary;
 * other bands are upper-inclusive, with a final overflow band. */
typedef struct sixel_repair_measure_record {
    unsigned int version;
    uint64_t sequence;
    uint64_t generation;
    uint64_t configuration_revision;
    uint64_t before_revision;
    uint64_t after_revision;
    SIXELSTATUS status;
    int outcome;
    int audit_complete;
    int selection_complete;
    int proposal_complete;
    int committed;
    int timing_valid;
    int workspace;
    int full_selection;
    int zero_noise;
    int rescue;
    /* Snapshot before counting this step's successful output. Empty knots
     * mean the initial resumable sort has not selected its first action. */
    sixel_repair_ladder_t ladder;
    double stop_squared;
    double mask_squared;
    double before_sum;
    double before_max;
    double before_excess;
    size_t eligible;
    size_t invalid;
    size_t unresolved;
    double band_upper[SIXEL_REPAIR_MEASURE_BANDS - 1];
    size_t bands[SIXEL_REPAIR_MEASURE_BANDS];
    double band_sums[SIXEL_REPAIR_MEASURE_BANDS];
    size_t sample_stride;
    size_t sample_count;
    sixel_repair_measure_sample_t samples[SIXEL_REPAIR_MEASURE_SAMPLES];
    /* Exact band populations and paired, order-independent bottom-k samples.
     * Valid only with audit_complete and the recorded before_revision. */
    sixel_repair_predict_model_t prediction_model;
    size_t selected;
    size_t packet_limit;
    size_t pixel_limit;
    size_t distance_limit;
    unsigned int color_limit;
    size_t proposal_pixels;
    double proposal_gain;
    size_t proposal_bytes;
    size_t fitting_retries;
    size_t committed_pixels;
    size_t committed_bytes;
    double committed_gain;
    double committed_excess_gain;
    size_t resolved_pixels;
    /* Published only after successful writer acceptance. No post-commit max
     * is inferred: the following complete audit provides that observation. */
    size_t transitions[SIXEL_REPAIR_MEASURE_BANDS]
                      [SIXEL_REPAIR_MEASURE_BANDS];
    unsigned int constructed_colors;
    unsigned int transmitted_colors;
    unsigned int palette_count;
    double noise_strength;
    float palette[255][3];
    uint64_t observer_distances;
    sixel_repair_measure_cost_t call[SIXEL_REPAIR_MEASURE_PHASES];
} sixel_repair_measure_record_t;

struct sixel_repair_measure_state;
struct sixel_repair_result;

/* Enable before the first bind. Read copies the latest completed step while
 * holding the encoder gate; callers perform I/O afterwards. Sequence gaps
 * identify overwritten observations. Rebinding clears the previous record.
 * Disabled measurement adds no clocks, scans, allocations or distance work. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_measure_enable(sixel_progressive_t *context);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_measure_read(sixel_progressive_t *context,
    sixel_repair_measure_record_t *record);

/* All hooks below require encoder ownership; none invokes user callbacks. */
SIXEL_INTERNAL_API void
sixel_repair_measure_rebind(sixel_progressive_t *context);
SIXEL_INTERNAL_API void
sixel_repair_measure_begin(sixel_progressive_t *context,
    sixel_repair_work_t const *work);
SIXEL_INTERNAL_API void
sixel_repair_measure_phase(sixel_progressive_t *context, int phase,
    sixel_repair_work_t const *work);
SIXEL_INTERNAL_API void
sixel_repair_measure_audit_begin(sixel_progressive_t *context);
SIXEL_INTERNAL_API void
sixel_repair_measure_pixel(sixel_progressive_t *context, size_t position,
    float const *target, double error);
SIXEL_INTERNAL_API void
sixel_repair_measure_packet_begin(sixel_progressive_t *context);
SIXEL_INTERNAL_API void
sixel_repair_measure_proposal(sixel_progressive_t *context, size_t position);
SIXEL_INTERNAL_API void
sixel_repair_measure_serialized(sixel_progressive_t *context);
SIXEL_INTERNAL_API void
sixel_repair_measure_commit_pixel(sixel_progressive_t *context,
    size_t position);
SIXEL_INTERNAL_API void
sixel_repair_measure_finish(sixel_progressive_t *context,
    sixel_repair_work_t const *work, struct sixel_repair_result const *result,
    SIXELSTATUS status);

#endif
