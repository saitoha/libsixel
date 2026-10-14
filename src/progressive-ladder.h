/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_PROGRESSIVE_LADDER_H
#define LIBSIXEL_PROGRESSIVE_LADDER_H

#include <sixel.h>

#define SIXEL_REPAIR_LADDER_RUNGS 5

enum {
    SIXEL_REPAIR_LADDER_INITIAL,
    SIXEL_REPAIR_LADDER_PACKET_LIMIT,
    SIXEL_REPAIR_LADDER_EMPTY_MASK,
    SIXEL_REPAIR_LADDER_NO_PAINT
};

/* Private experimental policy; allowance zero preserves legacy selection.
 * Thresholds are squared -W distances, frozen for one source binding.
 * Emissions count accepted packets at the current nonfinal rung only. */
typedef struct sixel_repair_ladder {
    unsigned int allowance;
    unsigned int count;
    unsigned int rung;
    unsigned int emissions;
    unsigned int reason;
    int noise_off;
    double thresholds[SIXEL_REPAIR_LADDER_RUNGS];
} sixel_repair_ladder_t;

/* Build once from the first complete audit's descending unresolved indices.
 * Equal knots and the initial minimum (already the full mask) collapse;
 * the final rung is always the exact stop target, including a zero target. */
SIXEL_INTERNAL_API void
sixel_repair_ladder_build(sixel_repair_ladder_t *ladder,
    double const *errors, size_t const *order, size_t count, double stop);
SIXEL_INTERNAL_API int
sixel_repair_ladder_descend(sixel_repair_ladder_t *ladder,
    unsigned int reason);
/* Call only after successful writer acceptance, never for yields, refits,
 * cancellation before admission or a failed output callback. */
SIXEL_INTERNAL_API void
sixel_repair_ladder_commit(sixel_repair_ladder_t *ladder);

/* Configure before the first bind, under the same encoder operation gate as
 * other mutations. Rebind preserves the allowance but resets all progress. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_repair_ladder_enable(sixel_progressive_t *context,
    unsigned int allowance);

#endif
