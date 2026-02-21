/*
 * SPDX-License-Identifier: MIT
 */

#ifndef SIXEL_SRC_TIMER_H
#define SIXEL_SRC_TIMER_H

#include <sixel.h>

/*
 * Monotonic wall clock helper for profiling and logging.
 * The timer is exposed separately from the assessment pipeline so that
 * lightweight frontends can measure durations without pulling in the
 * full assessment stack.
 */
SIXEL_INTERNAL_API double sixel_timer_now(void);

#endif
