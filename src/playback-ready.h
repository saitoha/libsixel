/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_PLAYBACK_READY_H
#define LIBSIXEL_PLAYBACK_READY_H
#include <limits.h>
#include <stdint.h>
#include "sixel_atomic.h"
#include "threading.h"

struct sixel_encoder_pipeline_slot;

/* One producer publishes descriptors; one consumer removes them. Removal
 * does not release the slot: a separate release flag protects its resources.
 * Native atomic implementations provide the fast handoff; the existing
 * mutex atomic fallback preserves ordering on other threaded targets. */
typedef struct sixel_playback_ready {
    struct sixel_encoder_pipeline_slot *entries[2];
    sixel_atomic_u32_t head, tail;
    sixel_atomic_u32_t released[2];
    sixel_atomic_u32_t credit, stop;
    sixel_mutex_t gate;
    sixel_cond_t changed;
} sixel_playback_ready_t;

static inline unsigned int
sixel_playback_ready_load(sixel_atomic_u32_t *value)
{
    return sixel_atomic_fetch_add_u32(value, 0);
}

static inline int
sixel_playback_ready_push(sixel_playback_ready_t *q,
    struct sixel_encoder_pipeline_slot *slot)
{
    unsigned int head, tail;

    tail = sixel_playback_ready_load(&q->tail);
    head = sixel_playback_ready_load(&q->head);
    /* Explicit modulo projection also makes wraparound sanitizer-clean. */
    if ((unsigned int)((uint64_t)tail + UINT_MAX + 1u - head) == 2)
        return 0;
    q->entries[tail % 2] = slot;
    /* Release publishes every job field before the consumer acquires tail. */
    (void)sixel_atomic_fetch_add_u32(&q->tail, 1);
    return 1;
}

static inline int
sixel_playback_ready_pop(sixel_playback_ready_t *q,
    struct sixel_encoder_pipeline_slot **slot)
{
    unsigned int head, tail;

    head = sixel_playback_ready_load(&q->head);
    tail = sixel_playback_ready_load(&q->tail);
    if (head == tail) return 0;
    *slot = q->entries[head % 2];
    (void)sixel_atomic_fetch_add_u32(&q->head, 1);
    return 1;
}

static inline void
sixel_playback_ready_wake(sixel_playback_ready_t *q)
{
    sixel_mutex_lock(&q->gate);
    sixel_cond_broadcast(&q->changed);
    sixel_mutex_unlock(&q->gate);
}

/* Credit is binary: only the current frame's completed resolve grants the
 * next solve. No future solve can queue in front of that resolve. */
static inline void
sixel_playback_ready_credit(void *context)
{
    sixel_playback_ready_t *q;

    q = context;
    (void)sixel_atomic_fetch_add_u32(&q->credit, 1);
    sixel_playback_ready_wake(q);
}

static inline void
sixel_playback_ready_stop(sixel_playback_ready_t *q)
{
    (void)sixel_atomic_fetch_add_u32(&q->stop, 1);
    sixel_playback_ready_wake(q);
}
#endif
