/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "palette_store_test_common.h"
#include "palette_stage2_common.h"
#include "src/allocator.h"

int
palette_store_fixture_init(palette_store_fixture_t *fixture)
{
    int i;

    memset(fixture, 0, sizeof(*fixture));
    for (i = 0; i < 2; ++i) {
        if (sixel_mutex_init(&fixture->gates[i].mutex) != 0) {
            return 1;
        }
        if (sixel_cond_init(&fixture->gates[i].cond) != 0) {
            sixel_mutex_destroy(&fixture->gates[i].mutex);
            return 1;
        }
        ++fixture->initialized;
    }
    return SIXEL_FAILED(sixel_allocator_new(&fixture->allocator,
        NULL, NULL, NULL, NULL)) ||
        SIXEL_FAILED(sixel_palette_store_new(&fixture->store,
            fixture->allocator));
}

void
palette_store_gate_wait(palette_store_gate_t *gate, int *field)
{
    sixel_mutex_lock(&gate->mutex);
    while (!*field) {
        sixel_cond_wait(&gate->cond, &gate->mutex);
    }
    sixel_mutex_unlock(&gate->mutex);
}

void
palette_store_gate_open(palette_store_gate_t *gate, int leave)
{
    sixel_mutex_lock(&gate->mutex);
    gate->open = 1;
    gate->leave |= leave;
    sixel_cond_broadcast(&gate->cond);
    sixel_mutex_unlock(&gate->mutex);
}

static void
palette_store_test_before_publish(void *context)
{
    palette_store_gate_t *gate;

    gate = context;
    sixel_mutex_lock(&gate->mutex);
    gate->entered = 1;
    sixel_cond_broadcast(&gate->cond);
    while (!gate->open) {
        sixel_cond_wait(&gate->cond, &gate->mutex);
    }
    sixel_mutex_unlock(&gate->mutex);
    sixel_helper_set_additional_message("palette store completion fault");
}

static void
palette_store_test_after_publish(void *context)
{
    palette_store_gate_t *gate;

    gate = context;
    sixel_mutex_lock(&gate->mutex);
    gate->published = 1;
    sixel_cond_broadcast(&gate->cond);
    while (!gate->leave) {
        sixel_cond_wait(&gate->cond, &gate->mutex);
    }
    sixel_mutex_unlock(&gate->mutex);
}

static void
palette_store_test_before_join(void *context)
{
    palette_store_gate_t *gate;

    gate = context;
    sixel_mutex_lock(&gate->mutex);
    gate->join = 1;
    sixel_cond_broadcast(&gate->cond);
    sixel_mutex_unlock(&gate->mutex);
}

SIXELSTATUS
palette_store_test_admit(palette_store_fixture_t *fixture, uint64_t sequence,
    palette_store_gate_t *gate, SIXELSTATUS fault,
    sixel_palette_store_job_t **job)
{
    sixel_palette_build_request_t *request;
    sixel_frame_t *frame;
    sixel_palette_job_test_hooks_t hooks;
    SIXELSTATUS status;

    *job = NULL;
    if (palette_stage2_request(fixture->allocator, 0, SIXEL_LUT_POLICY_NONE,
            0, 0, &request, &frame)) {
        return SIXEL_RUNTIME_ERROR;
    }
    memset(&hooks, 0, sizeof(hooks));
    hooks.completion_status = fault;
    if (gate != NULL) {
        hooks.before_publish = palette_store_test_before_publish;
        hooks.after_publish = palette_store_test_after_publish;
        hooks.before_join = palette_store_test_before_join;
        hooks.context = gate;
    }
    status = sixel_palette_store_admit(fixture->store, sequence, &request,
        &frame, &hooks, job);
    sixel_palette_build_request_free(request);
    sixel_frame_unref(frame);
    return status;
}

int
palette_store_test_select(sixel_palette_store_t *store, uint64_t sequence,
    uint64_t expected, sixel_palette_selection_t *retained)
{
    sixel_palette_selection_t result;
    int failed;

    failed = sixel_palette_store_select(store, sequence, &result) != SIXEL_OK;
    failed |= result.sequence != expected;
    failed |= (result.snapshot != NULL) != (expected != 0);
    if (expected != 0) {
        failed |= result.epoch == 0 || result.attempt == 0 ||
            result.published == 0 || result.published >= result.selected;
    }
    if (retained != NULL) {
        *retained = result;
    } else {
        sixel_palette_snapshot_unref(result.snapshot);
    }
    return failed;
}

int
palette_store_fixture_free(palette_store_fixture_t *fixture)
{
    int i;
    int failed;

    for (i = 0; i < fixture->initialized; ++i) {
        palette_store_gate_open(&fixture->gates[i], 1);
    }
    sixel_palette_store_free(fixture->store);
    failed = fixture->allocator != NULL &&
        sixel_atomic_fetch_add_u32(&fixture->allocator->ref, 0) != 1;
    sixel_allocator_unref(fixture->allocator);
    for (i = 0; i < fixture->initialized; ++i) {
        sixel_cond_destroy(&fixture->gates[i].cond);
        sixel_mutex_destroy(&fixture->gates[i].mutex);
    }
    return failed;
}
