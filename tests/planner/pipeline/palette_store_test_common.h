/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_PALETTE_STORE_TEST_COMMON_H
#define LIBSIXEL_PALETTE_STORE_TEST_COMMON_H
#include <stdio.h>
#include "src/encoder-palette-store.h"
#include "src/threading.h"

#define PALETTE_STORE_CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
        goto end; \
    } \
} while (0)

typedef struct palette_store_gate {
    sixel_mutex_t mutex;
    sixel_cond_t cond;
    int entered;
    int open;
    int published;
    int leave;
    int join;
    int threads;
    int palette_mode;
} palette_store_gate_t;

typedef struct palette_store_fixture {
    sixel_allocator_t *allocator;
    sixel_palette_store_t *store;
    sixel_palette_scheduler_t scheduler;
    int scheduler_initialized;
    palette_store_gate_t gates[2];
    int initialized;
} palette_store_fixture_t;

int palette_store_fixture_init(palette_store_fixture_t *fixture);
int palette_store_fixture_free(palette_store_fixture_t *fixture);
SIXELSTATUS palette_store_test_admit(palette_store_fixture_t *fixture,
    uint64_t sequence, palette_store_gate_t *gate, SIXELSTATUS fault,
    sixel_palette_store_job_t **job);
int palette_store_test_select(sixel_palette_store_t *store, uint64_t sequence,
    uint64_t expected, sixel_palette_selection_t *retained);
void palette_store_gate_wait(palette_store_gate_t *gate, int *field);
void palette_store_gate_open(palette_store_gate_t *gate, int leave);
#endif
