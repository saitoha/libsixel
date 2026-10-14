/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "progressive-ladder.h"

void
sixel_repair_ladder_build(sixel_repair_ladder_t *ladder,
    double const *errors, size_t const *order, size_t count, double stop)
{
    size_t ranks[4];
    unsigned int i;
    double value;

    if (ladder->count != 0) {
        return;
    }
    if (count != 0) {
        /* Exact ceil ranks avoid floating-point rounding and multiplication
         * overflow. Quantiles 7/8, 3/4, 1/2, 1/4 mean top 1/8..3/4 masks. */
        ranks[0] = count / 8 + (count % 8 != 0);
        ranks[1] = count / 4 + (count % 4 != 0);
        ranks[2] = count / 2 + (count % 2 != 0);
        ranks[3] = count - count / 4;
        for (i = 0; i < 4; ++i) {
            value = errors[order[ranks[i] - 1]];
            if (value > stop && value > errors[order[count - 1]] &&
                    (ladder->count == 0 ||
                     value < ladder->thresholds[ladder->count - 1])) {
                ladder->thresholds[ladder->count++] = value;
            }
        }
    }
    ladder->thresholds[ladder->count++] = stop;
    ladder->noise_off |= ladder->count == 1;
}

int
sixel_repair_ladder_descend(sixel_repair_ladder_t *ladder,
    unsigned int reason)
{
    if (ladder->count == 0 || ladder->rung + 1 >= ladder->count) {
        return 0;
    }
    ++ladder->rung;
    ladder->reason = reason;
    ladder->emissions = 0;
    ladder->noise_off |= ladder->rung + 1 == ladder->count;
    return 1;
}

void
sixel_repair_ladder_commit(sixel_repair_ladder_t *ladder)
{
    if (ladder->allowance && ladder->count &&
            ladder->rung + 1 < ladder->count) {
        if (++ladder->emissions == ladder->allowance) {
            (void)sixel_repair_ladder_descend(ladder,
                SIXEL_REPAIR_LADDER_PACKET_LIMIT);
        }
    }
}
