/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "progressive-select.h"

enum {
    REPAIR_COLLECT,
    REPAIR_SORT_ERRORS,
    REPAIR_SELECT,
    REPAIR_PROJECT,
    REPAIR_DEDUP,
    REPAIR_ORDER_INIT,
    REPAIR_SORT_COLORS,
    REPAIR_POOL,
    REPAIR_FARTHEST,
    REPAIR_SELECTION_DONE
};

void
sixel_repair_selection_free(sixel_repair_selection_t *selection)
{
    sixel_allocator_t *allocator;

    if (selection == NULL) {
        return;
    }
    allocator = selection->allocator;
    sixel_allocator_free(allocator, selection->indices);
    sixel_allocator_free(allocator, selection->scratch);
    sixel_allocator_free(allocator, selection->selected);
    sixel_allocator_free(allocator, selection->samples);
    sixel_allocator_free(allocator, selection->raw);
    sixel_allocator_free(allocator, selection->hash);
    sixel_allocator_free(allocator, selection->colors);
    sixel_allocator_free(allocator, selection->working);
    sixel_allocator_free(allocator, selection);
    sixel_allocator_unref(allocator);
}

SIXELSTATUS
sixel_repair_selection_new(sixel_repair_selection_t **selection,
    float const *source, double const *errors, unsigned char const *domain,
    size_t count, sixel_palette_t const *transform, int workspace,
    int palette_type, double stop_squared, double fraction, int full,
    size_t sample_limit, unsigned int candidate_limit,
    sixel_allocator_t *allocator)
{
    sixel_repair_selection_t *created;
    size_t samples;
    size_t hash_size;

    if (selection == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *selection = NULL;
    if (source == NULL || errors == NULL || domain == NULL || count == 0 ||
            transform == NULL || allocator == NULL ||
            count > SIZE_MAX / sizeof(size_t) ||
            count > SIZE_MAX / (3 * sizeof(float)) ||
            !isfinite(stop_squared) || stop_squared < 0 ||
            !isfinite(fraction) || fraction <= 0 || fraction > 1 ||
            sample_limit == 0 || sample_limit > 65536 ||
            candidate_limit == 0 || candidate_limit > 4096) {
        return SIXEL_BAD_ARGUMENT;
    }
    samples = count < sample_limit ? count : sample_limit;
    hash_size = 1;
    while (hash_size < (samples + 1) * 2) {
        hash_size *= 2;
    }
    created = sixel_allocator_calloc(allocator, 1, sizeof(*created));
    if (created == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    sixel_allocator_ref(allocator);
    created->allocator = allocator;
    created->source = source;
    created->errors = errors;
    created->domain = domain;
    created->count = count;
    created->transform = transform;
    created->workspace = workspace;
    created->palette_type = palette_type;
    created->stop_squared = stop_squared;
    created->fraction = fraction;
    created->full = full;
    created->sample_limit = samples;
    created->candidate_limit = candidate_limit;
    created->hash_size = hash_size;
    created->protected_position = SIZE_MAX;
    created->protected_raw = SIZE_MAX;
    created->protected_color = -1;
    created->indices = sixel_allocator_malloc(allocator,
        count * sizeof(*created->indices));
    created->scratch = sixel_allocator_malloc(allocator,
        count * sizeof(*created->scratch));
    created->selected = sixel_allocator_malloc(allocator, count);
    created->samples = sixel_allocator_calloc(allocator, samples,
        sizeof(*created->samples));
    created->raw = sixel_allocator_calloc(allocator, samples + 1,
        sizeof(*created->raw));
    created->hash = sixel_allocator_calloc(allocator, hash_size,
        sizeof(*created->hash));
    created->colors = sixel_allocator_malloc(allocator,
        candidate_limit * sizeof(*created->colors));
    created->working = sixel_allocator_malloc(allocator,
        candidate_limit * sizeof(*created->working));
    if (created->indices == NULL || created->scratch == NULL ||
            created->selected == NULL || created->samples == NULL ||
            created->raw == NULL || created->hash == NULL ||
            created->colors == NULL || created->working == NULL) {
        sixel_repair_selection_free(created);
        return SIXEL_BAD_ALLOCATION;
    }
    *selection = created;
    return SIXEL_OK;
}

/* Bottom-up stable merging consumes one visit for each placed index. The
 * two orderings share storage: residual descending, or provenance ascending.
 * A tie always resolves to the earliest source position. */
static int
sixel_repair_selection_sort(sixel_repair_selection_t *selection,
                            int colors)
{
    size_t count;
    size_t a;
    size_t b;
    size_t *swap;
    int take_left;

    count = colors ? selection->raw_count : selection->unresolved;
    if (selection->width >= count) {
        return 1;
    }
    if (selection->destination == selection->finish) {
        selection->block = selection->finish;
        if (selection->block == count) {
            swap = selection->indices;
            selection->indices = selection->scratch;
            selection->scratch = swap;
            selection->width = selection->width > count / 2
                ? count : selection->width * 2;
            selection->block = 0;
            if (selection->width >= count) {
                return 1;
            }
        }
        selection->left = selection->block;
        selection->middle = count - selection->block < selection->width
            ? count : selection->block + selection->width;
        selection->right = selection->middle;
        selection->finish = count - selection->middle < selection->width
            ? count : selection->middle + selection->width;
        selection->destination = selection->block;
    }
    take_left = selection->right == selection->finish;
    if (selection->left == selection->middle) {
        take_left = 0;
    } else if (!take_left) {
        a = selection->indices[selection->left];
        b = selection->indices[selection->right];
        take_left = colors
            ? selection->raw[a].position <= selection->raw[b].position
            : selection->errors[a] > selection->errors[b] ||
              (selection->errors[a] == selection->errors[b] && a < b);
    }
    selection->scratch[selection->destination++] = selection->indices[
        take_left ? selection->left++ : selection->right++];
    return 0;
}

static void
sixel_repair_selection_sort_begin(sixel_repair_selection_t *selection)
{
    selection->width = 1;
    selection->block = 0;
    selection->destination = 0;
    selection->finish = 0;
}

/* The remainder product is at most 65536 squared. Widen it explicitly for
 * 32-bit size_t, while the quotient product is bounded by selected_count. */
static size_t
sixel_repair_stratum_boundary(sixel_repair_selection_t const *selection,
                              size_t ordinal)
{
    size_t count;
    size_t samples;

    count = selection->selected_count;
    samples = selection->sample_count;
    return ordinal * (count / samples) +
        (size_t)((uint64_t)ordinal * (count % samples) / samples);
}

static size_t
sixel_repair_candidate_hash(sixel_palette_emitted_color_t const *color)
{
    uint32_t hash;
    int channel;

    hash = (uint32_t)color->type;
    for (channel = 0; channel < 3; ++channel) {
        hash = hash * UINT32_C(16777619) ^ (uint32_t)color->fields[channel];
    }
    return hash;
}

static int
sixel_repair_candidate_equal(sixel_palette_emitted_color_t const *a,
                              sixel_palette_emitted_color_t const *b)
{
    return a->type == b->type && a->fields[0] == b->fields[0] &&
        a->fields[1] == b->fields[1] && a->fields[2] == b->fields[2];
}

/* Selection requests every eligible position in a bounded batch. Projection
 * instead requests the strata of the complete, canonically corrected mask.
 * Thus neither sample provenance nor weight depends on the step budget. */
static SIXELSTATUS
sixel_repair_selection_scan(sixel_repair_selection_t *selection,
    sixel_repair_work_t *work)
{
    sixel_repair_gpu_request_t request;
    unsigned char filter[SIXEL_REPAIR_GPU_SAMPLE_MAX];
    size_t count;
    size_t remaining;
    size_t i;
    size_t position;
    SIXELSTATUS status;

    count = selection->count - selection->cursor;
    if (count > SIXEL_REPAIR_GPU_SAMPLE_MAX) {
        count = SIXEL_REPAIR_GPU_SAMPLE_MAX;
    }
    remaining = work->pixel_limit - work->pixel_visits;
    if (count > remaining) count = remaining;
    remaining = work->distance_limit - work->distance_evaluations;
    if (count > remaining) count = remaining;
    memset(&request, 0, sizeof(request));
    request.view = selection->gpu_view;
    request.policy = selection->gpu_policy;
    request.first = selection->cursor;
    request.count = count;
    request.sample_limit = SIXEL_REPAIR_GPU_SAMPLE_MAX;
    request.stop_squared = selection->stop_squared;
    request.selection_squared = selection->threshold;
    request.filter = filter;
    request.preselected = selection->stage == REPAIR_PROJECT;
    if (request.preselected) {
        request.sample_population = selection->selected_count;
        request.sample_offset = selection->ordinal;
        request.sample_total = (unsigned int)selection->sample_count;
        request.sample_first = (unsigned int)selection->sample;
    }
    for (i = 0; i < count; ++i) {
        position = selection->cursor + i;
        filter[i] = request.preselected ? selection->selected[position]
            : 1;
    }
    /* A failed attempt has spent its admitted work too. Do not advance the
     * CPU cursor or reuse that allowance when AUTO retries on the CPU. */
    work->pixel_visits += count;
    work->distance_evaluations += count;
    status = sixel_repair_gpu_scan(selection->gpu, &request,
        &selection->gpu_result);
    if (status == SIXEL_FALSE) {
        selection->gpu_policy = SIXEL_GPU_POLICY_OFF;
        return SIXEL_OK;
    }
    if (status == SIXEL_OK) {
        selection->gpu_used = 1;
        selection->gpu_end = selection->cursor + count;
        selection->gpu_sample = 0;
    }
    return status;
}

SIXELSTATUS
sixel_repair_selection_advance(sixel_repair_selection_t *selection,
    sixel_repair_work_t *work, int *done)
{
    sixel_repair_candidate_t candidate;
    sixel_repair_candidate_t *entry;
    sixel_repair_sample_t *sample;
    sixel_repair_gpu_sample_t const *gpu_sample;
    size_t start;
    size_t finish;
    size_t position;
    size_t index;
    size_t rank;
    unsigned int chunk;
    double distance;
    double error;
    int selected;
    int proposed;

    if (selection == NULL || work == NULL || done == NULL ||
            work->pixel_limit == 0 || work->distance_limit == 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    *done = 0;
    for (chunk = 0; chunk < 256; ++chunk) {
        if (selection->stage == REPAIR_SELECTION_DONE) {
            *done = 1;
            return SIXEL_OK;
        }
        if (work->pixel_visits >= work->pixel_limit ||
                work->distance_evaluations >= work->distance_limit) {
            return SIXEL_OK;
        }
        if (selection->gpu_policy != SIXEL_GPU_POLICY_OFF &&
                (selection->stage == REPAIR_SELECT ||
                 selection->stage == REPAIR_PROJECT) &&
                selection->cursor >= selection->gpu_end) {
            /* Return after device completion so cancellation and ordinary
             * work get an admission checkpoint before consuming proposals. */
            return sixel_repair_selection_scan(selection, work);
        }
        ++work->pixel_visits;
        switch (selection->stage) {
        case REPAIR_COLLECT:
            position = selection->cursor++;
            if (!isfinite(selection->errors[position]) ||
                    selection->errors[position] < 0) {
                return SIXEL_BAD_INPUT;
            }
            if (selection->domain[position] &&
                    selection->errors[position] > selection->stop_squared) {
                selection->indices[selection->unresolved++] = position;
            }
            if (selection->cursor == selection->count) {
                selection->cursor = 0;
                sixel_repair_selection_sort_begin(selection);
                selection->stage = selection->full || !selection->unresolved
                    ? REPAIR_SELECT : REPAIR_SORT_ERRORS;
                selection->threshold = selection->stop_squared;
                if (selection->ladder != NULL && selection->ladder->count) {
                    selection->threshold = selection->ladder->thresholds[
                        selection->ladder->rung];
                    selection->stage = REPAIR_SELECT;
                }
            }
            break;
        case REPAIR_SORT_ERRORS:
            if (sixel_repair_selection_sort(selection, 0)) {
                /* count is bounded by addressable float RGB storage, so
                 * ceil's finite result fits size_t. Clamp double rounding. */
                rank = (size_t)ceil(selection->unresolved *
                                    selection->fraction);
                if (rank < 1) {
                    rank = 1;
                } else if (rank > selection->unresolved) {
                    rank = selection->unresolved;
                }
                selection->threshold = selection->errors[
                    selection->indices[rank - 1]];
                if (selection->ladder != NULL) {
                    sixel_repair_ladder_build(selection->ladder,
                        selection->errors, selection->indices,
                        selection->unresolved, selection->stop_squared);
                    selection->threshold = selection->ladder->thresholds[
                        selection->ladder->rung];
                }
                selection->stage = REPAIR_SELECT;
            }
            break;
        case REPAIR_SELECT:
            position = selection->cursor++;
            selected = selection->domain[position] &&
                selection->errors[position] > selection->stop_squared &&
                selection->errors[position] >= selection->threshold;
            if (selection->gpu_policy != SIXEL_GPU_POLICY_OFF) {
                proposed = selection->gpu_sample <
                    selection->gpu_result.summary.sample_count &&
                    selection->gpu_result.samples[
                        selection->gpu_sample].position == position;
                selection->gpu_sample += proposed;
                /* Float thresholds can cross canonical double ties. The
                 * audited predicate also restores omitted tiny residuals;
                 * a GPU miss must not starve a protected candidate. */
                selection->selected[position] = proposed;
                if (proposed != selected) {
                    selection->selected[position] = selected;
                }
            } else {
                selection->selected[position] = selected;
            }
            selection->selected_count += selection->selected[position];
            if (selection->cursor == selection->count) {
                selection->cursor = 0;
                selection->gpu_end = 0;
                selection->sample_count = selection->selected_count <
                    selection->sample_limit ? selection->selected_count :
                    selection->sample_limit;
                selection->stage = selection->sample_count
                    ? REPAIR_PROJECT : REPAIR_SELECTION_DONE;
            }
            break;
        case REPAIR_PROJECT:
            position = selection->cursor++;
            if (selection->selected[position]) {
                memset(&candidate, 0, sizeof(candidate));
                candidate.position = position;
                candidate.error = selection->errors[position];
                candidate.nearest = DBL_MAX;
                if (!sixel_palette_project_output(selection->transform,
                        selection->source + position * 3,
                        selection->workspace, selection->palette_type,
                        &candidate.color)) {
                    return SIXEL_BAD_INPUT;
                }
                distance = sixel_repair_distance2(
                    selection->source + position * 3,
                    candidate.color.working);
                ++work->distance_evaluations;
                if (sixel_repair_improves(candidate.error, distance) &&
                        (selection->protected_position == SIZE_MAX ||
                         candidate.error > selection->protected.error)) {
                    selection->protected = candidate;
                    selection->protected_position = position;
                }
                start = finish = 0;
                gpu_sample = NULL;
                if (selection->gpu_policy != SIXEL_GPU_POLICY_OFF) {
                    if (selection->gpu_sample <
                            selection->gpu_result.summary.sample_count &&
                            selection->gpu_result.samples[
                                selection->gpu_sample].position == position) {
                        gpu_sample = selection->gpu_result.samples +
                            selection->gpu_sample++;
                        finish = (size_t)gpu_sample->population;
                    }
                } else if (selection->sample < selection->sample_count) {
                    start = sixel_repair_stratum_boundary(selection,
                        selection->sample);
                    finish = sixel_repair_stratum_boundary(selection,
                        selection->sample + 1);
                }
                if (gpu_sample != NULL || (finish > start &&
                        selection->ordinal ==
                            start + (finish - start - 1) / 2)) {
                    sample = selection->samples + selection->sample;
                    memcpy(sample->source, gpu_sample != NULL
                        ? gpu_sample->source : selection->source +
                            position * 3, sizeof(sample->source));
                    sample->position = position;
                    sample->error = candidate.error;
                    sample->population = finish - start;
                    selection->raw[selection->sample++] = candidate;
                }
                ++selection->ordinal;
            }
            if (selection->cursor == selection->count) {
                selection->raw_total = selection->sample_count;
                if (selection->protected_position != SIZE_MAX) {
                    selection->raw[selection->raw_total++] =
                        selection->protected;
                }
                selection->cursor = 0;
                selection->stage = REPAIR_DEDUP;
            }
            break;
        case REPAIR_DEDUP:
            if (!selection->hashing) {
                selection->pending = selection->raw[selection->cursor];
                selection->hash_slot = sixel_repair_candidate_hash(
                    &selection->pending.color) & (selection->hash_size - 1);
                selection->hashing = 1;
            }
            index = selection->hash[selection->hash_slot];
            if (index == 0) {
                index = selection->raw_count++;
                selection->raw[index] = selection->pending;
                selection->hash[selection->hash_slot] = index + 1;
            } else {
                --index;
                if (!sixel_repair_candidate_equal(
                        &selection->raw[index].color,
                        &selection->pending.color)) {
                    selection->hash_slot = (selection->hash_slot + 1) &
                        (selection->hash_size - 1);
                    break;
                }
                entry = selection->raw + index;
                if (entry->position > selection->pending.position) {
                    entry->position = selection->pending.position;
                }
                if (entry->error < selection->pending.error) {
                    entry->error = selection->pending.error;
                }
            }
            if (selection->pending.position ==
                    selection->protected_position) {
                selection->protected_raw = index;
            }
            selection->hashing = 0;
            if (++selection->cursor == selection->raw_total) {
                selection->cursor = 0;
                selection->stage = REPAIR_ORDER_INIT;
            }
            break;
        case REPAIR_ORDER_INIT:
            selection->indices[selection->cursor] = selection->cursor;
            if (++selection->cursor == selection->raw_count) {
                selection->cursor = 0;
                sixel_repair_selection_sort_begin(selection);
                selection->stage = REPAIR_SORT_COLORS;
            }
            break;
        case REPAIR_SORT_COLORS:
            if (sixel_repair_selection_sort(selection, 1)) {
                selection->stage = REPAIR_POOL;
                selection->chosen = selection->protected_raw;
                selection->best = SIZE_MAX;
            }
            break;
        case REPAIR_POOL:
            /* A pool that fits retains provenance order. Only reduction
             * changes it to protected/maximum-error then farthest-first. */
            if (selection->raw_count <= selection->candidate_limit) {
                index = selection->indices[selection->cursor++];
            } else if (selection->chosen != SIZE_MAX) {
                index = selection->chosen;
            } else {
                index = selection->indices[selection->cursor++];
                if (selection->best == SIZE_MAX ||
                        selection->raw[index].error >
                            selection->raw[selection->best].error) {
                    selection->best = index;
                }
                if (selection->cursor < selection->raw_count) {
                    break;
                }
                index = selection->best;
            }
            selection->colors[selection->color_count] =
                selection->raw[index].color;
            memcpy(selection->working[selection->color_count],
                selection->raw[index].color.working,
                sizeof(selection->working[0]));
            if (index == selection->protected_raw) {
                selection->protected_color = (int)selection->color_count;
            }
            ++selection->color_count;
            selection->raw[index].used = 1;
            if (selection->color_count == selection->candidate_limit ||
                    selection->color_count == selection->raw_count) {
                selection->stage = REPAIR_SELECTION_DONE;
            } else if (selection->raw_count > selection->candidate_limit) {
                selection->cursor = 0;
                selection->best = SIZE_MAX;
                selection->stage = REPAIR_FARTHEST;
            }
            break;
        case REPAIR_FARTHEST:
            index = selection->indices[selection->cursor++];
            entry = selection->raw + index;
            if (!entry->used) {
                distance = sixel_repair_distance2(entry->color.working,
                    selection->working[selection->color_count - 1]);
                ++work->distance_evaluations;
                if (distance < entry->nearest) {
                    entry->nearest = distance;
                }
                error = entry->nearest;
                if (selection->best == SIZE_MAX ||
                        error > selection->raw[selection->best].nearest) {
                    selection->best = index;
                }
            }
            if (selection->cursor == selection->raw_count) {
                selection->chosen = selection->best;
                selection->stage = REPAIR_POOL;
            }
            break;
        default:
            return SIXEL_LOGIC_ERROR;
        }
    }
    *done = selection->stage == REPAIR_SELECTION_DONE;
    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
