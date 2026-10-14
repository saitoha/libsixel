/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/progressive-gpu.h"

#define GPU_TEST_COUNT 527
#define GPU_REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "progressive GPU assertion at %d\n", __LINE__); \
        goto end; \
    } \
} while (0)

static int gpu_test_fail_allocation;

static void *
gpu_test_malloc(size_t size)
{
    return gpu_test_fail_allocation ? NULL : malloc(size);
}

static void *
gpu_test_calloc(size_t count, size_t size)
{
    return gpu_test_fail_allocation ? NULL : calloc(count, size);
}

/* An independent double reference checks deliberate-margin inputs. GPU
 * floats are proposals; near ties are not an exact-mask contract. */
static int
gpu_test_reference(sixel_repair_gpu_request_t const *request,
    sixel_repair_gpu_result_t const *result)
{
    size_t eligible[GPU_TEST_COUNT];
    size_t n;
    size_t invalid;
    size_t domain;
    size_t valid;
    size_t position;
    size_t maximum_position;
    size_t i;
    size_t j;
    size_t begin;
    size_t finish;
    double error;
    double difference;
    double maximum;
    double sum;
    sixel_repair_gpu_sample_t const *sample;

    n = invalid = domain = valid = maximum_position = 0;
    maximum = sum = 0;
    for (i = 0; i < request->count; ++i) {
        position = request->positions != NULL ? request->positions[i]
            : request->first + i;
        if (!request->view.domain[position]) continue;
        ++domain;
        if (!request->view.valid[position]) {
            ++invalid;
            continue;
        }
        error = 0;
        for (j = 0; j < 3; ++j) {
            difference = (double)request->view.source[position * 3 + j] -
                request->view.retained[position * 3 + j];
            error += difference * difference;
        }
        if (!valid || error > maximum) {
            maximum = error;
            maximum_position = position;
        }
        ++valid;
        sum += error;
        if (error > request->stop_squared &&
                error >= request->selection_squared) eligible[n++] = position;
    }
    if (result->summary.domain_count != domain ||
            result->summary.valid_count != valid ||
            result->summary.invalid_count != invalid ||
            result->summary.eligible_count != n ||
            result->summary.maximum != maximum ||
            result->summary.sum != sum || result->summary.nonfinite ||
            result->summary.maximum_position != maximum_position ||
            result->summary.sample_count !=
                (n < request->sample_limit ? n : request->sample_limit)) {
        return 0;
    }
    for (i = 0; i < result->summary.sample_count; ++i) {
        begin = i * n / result->summary.sample_count;
        finish = (i + 1) * n / result->summary.sample_count;
        position = eligible[begin + (finish - begin) / 2];
        sample = &result->samples[i];
        if (sample->position != position ||
                sample->plane_position !=
                    ((uint64_t)request->view.origin_y +
                     position / request->view.width) *
                    request->view.plane_width + request->view.origin_x +
                    position % request->view.width ||
                sample->population != finish - begin ||
                memcmp(sample->source, request->view.source + position * 3,
                    sizeof(sample->source)) != 0) return 0;
        error = 0;
        for (j = 0; j < 3; ++j) {
            difference = (double)sample->source[j] -
                request->view.retained[position * 3 + j];
            error += difference * difference;
        }
        if (sample->error != error) return 0;
    }
    return result->downloaded_bytes == sizeof(result->summary) +
        result->summary.sample_count * sizeof(result->samples[0]);
}

int
test_progressive_gpu_case(int mode)
{
    sixel_repair_gpu_t *cache;
    sixel_repair_gpu_t *other;
    sixel_repair_gpu_t *failed;
    sixel_allocator_t *allocator;
    sixel_repair_gpu_request_t request;
    sixel_repair_gpu_request_t invalid;
    sixel_repair_gpu_request_t other_request;
    sixel_repair_gpu_result_t result;
    sixel_repair_gpu_result_t saved;
    sixel_repair_gpu_result_t zero;
    float source[GPU_TEST_COUNT * 3];
    float history[GPU_TEST_COUNT * 3];
    float other_source[GPU_TEST_COUNT * 3];
    unsigned char domain[GPU_TEST_COUNT];
    unsigned char valid[GPU_TEST_COUNT];
    uint64_t positions[GPU_TEST_COUNT];
    size_t i;
    size_t total;
    unsigned int j;
    int rc;
    SIXELSTATUS status;

    cache = other = failed = NULL;
    allocator = NULL;
    rc = 1;
    memset(&request, 0, sizeof(request));
    memset(&zero, 0, sizeof(zero));
    gpu_test_fail_allocation = 0;
    for (i = 0; i < GPU_TEST_COUNT; ++i) {
        source[i * 3] = (float)(i % 5) * .125f;
        source[i * 3 + 1] = -.5f;
        source[i * 3 + 2] = 2;
        history[i * 3] = 0;
        history[i * 3 + 1] = -.25f;
        history[i * 3 + 2] = 1.75f;
        domain[i] = i % 7 != 0;
        valid[i] = i % 11 != 0;
        positions[i] = i;
    }
    request.view.source = source;
    request.view.retained = history;
    request.view.domain = domain;
    request.view.valid = valid;
    request.view.count = GPU_TEST_COUNT;
    request.view.width = 17;
    request.view.height = 31;
    request.view.origin_x = 7;
    request.view.origin_y = 13;
    request.view.plane_width = 101;
    request.view.plane_height = 53;
    request.view.workspace = SIXEL_COLORSPACE_OKLAB;
    request.view.source_generation = 1;
    request.view.history_revision = 1;
    request.view.configuration_revision = 1;
    request.count = GPU_TEST_COUNT;
    request.stop_squared = .125;
    request.selection_squared = .140625;
    request.sample_limit = SIXEL_REPAIR_GPU_SAMPLE_MAX;
    request.policy = SIXEL_GPU_POLICY_FORCE;
    GPU_REQUIRE(sixel_repair_gpu_new(&cache, NULL) == SIXEL_OK);
    if (mode == 158) {
        for (j = 0; j < 14; ++j) {
            invalid = request;
            switch (j) {
            case 0: invalid.view.width = 0; break;
            case 1: invalid.view.count = SIZE_MAX; break;
            case 2: invalid.view.origin_x = -1; break;
            case 3: invalid.view.origin_y = 100; break;
            case 4: invalid.count = SIXEL_REPAIR_GPU_BATCH_MAX + 1; break;
            case 5: invalid.first = GPU_TEST_COUNT; break;
            case 6: invalid.count = 0; break;
            case 7: invalid.sample_limit = 129; break;
            case 8: invalid.stop_squared = NAN; break;
            case 9: invalid.selection_squared = -1; break;
            case 10: invalid.view.source = NULL; break;
            case 11: invalid.policy = 3; break;
            case 12:
                invalid.positions = positions;
                invalid.first = 1;
                break;
            default:
                invalid.positions = positions;
                positions[9] = positions[8];
                break;
            }
            memset(&result, 1, sizeof(result));
            GPU_REQUIRE(sixel_repair_gpu_scan(cache, &invalid, &result)
                == SIXEL_BAD_ARGUMENT);
            GPU_REQUIRE(memcmp(&result, &zero, sizeof(result)) == 0);
        }
        rc = 0;
        goto end;
    }
    if (mode == 167) {
        GPU_REQUIRE(sixel_allocator_new(&allocator, gpu_test_malloc,
            gpu_test_calloc, realloc, free) == SIXEL_OK);
        gpu_test_fail_allocation = 1;
        GPU_REQUIRE(sixel_repair_gpu_new(&failed, allocator)
            == SIXEL_BAD_ALLOCATION);
        GPU_REQUIRE(failed == NULL);
        gpu_test_fail_allocation = 0;
        GPU_REQUIRE(sixel_repair_gpu_new(&failed, allocator) == SIXEL_OK);
        rc = 0;
        goto end;
    }
    if (mode == 159) {
        request.policy = SIXEL_GPU_POLICY_OFF;
        GPU_REQUIRE(sixel_repair_gpu_scan(cache, &request, &result)
            == SIXEL_FALSE);
        GPU_REQUIRE(memcmp(&result, &zero, sizeof(result)) == 0);
        request.policy = SIXEL_GPU_POLICY_AUTO;
        request.auto_threshold = request.count + 1;
        GPU_REQUIRE(sixel_repair_gpu_scan(cache, &request, &result)
            == SIXEL_FALSE);
        GPU_REQUIRE(memcmp(&result, &zero, sizeof(result)) == 0);
        request.policy = SIXEL_GPU_POLICY_FORCE;
    }
    status = sixel_repair_gpu_scan(cache, &request, &result);
    if (status == SIXEL_FEATURE_ERROR) {
        if (mode == 159) {
            request.policy = SIXEL_GPU_POLICY_AUTO;
            request.auto_threshold = request.count;
            GPU_REQUIRE(sixel_repair_gpu_scan(cache, &request, &result)
                == SIXEL_FALSE);
            GPU_REQUIRE(memcmp(&result, &zero, sizeof(result)) == 0);
        }
        rc = mode == 159 ? 0 : 77;
        goto end;
    }
    GPU_REQUIRE(status == SIXEL_OK);
    GPU_REQUIRE(gpu_test_reference(&request, &result));
    GPU_REQUIRE(result.uploaded_bytes == GPU_TEST_COUNT * 26);
    saved = result;
    if (mode == 159) {
        request.policy = SIXEL_GPU_POLICY_AUTO;
        request.auto_threshold = request.count;
        GPU_REQUIRE(sixel_repair_gpu_scan(cache, &request, &result)
            == SIXEL_OK);
        GPU_REQUIRE(gpu_test_reference(&request, &result));
    } else if (mode == 161) {
        request.count = 4;
        positions[0] = 1;
        positions[1] = 17;
        positions[2] = 257;
        positions[3] = GPU_TEST_COUNT - 1;
        request.positions = positions;
        GPU_REQUIRE(sixel_repair_gpu_scan(cache, &request, &result)
            == SIXEL_OK);
        GPU_REQUIRE(gpu_test_reference(&request, &result));
        GPU_REQUIRE(result.uploaded_bytes == 4 * sizeof(uint64_t));
    } else if (mode == 162) {
        GPU_REQUIRE(sixel_repair_gpu_scan(cache, &request, &result)
            == SIXEL_OK);
        GPU_REQUIRE(result.uploaded_bytes == 0);
        source[3] = .5;
        domain[2] = 0;
        ++request.view.source_generation;
        GPU_REQUIRE(sixel_repair_gpu_scan(cache, &request, &result)
            == SIXEL_OK);
        GPU_REQUIRE(result.uploaded_bytes == GPU_TEST_COUNT * 13);
        GPU_REQUIRE(gpu_test_reference(&request, &result));
        history[4] = -.5;
        valid[3] = 0;
        ++request.view.history_revision;
        GPU_REQUIRE(sixel_repair_gpu_scan(cache, &request, &result)
            == SIXEL_OK);
        GPU_REQUIRE(result.uploaded_bytes == GPU_TEST_COUNT * 13);
        GPU_REQUIRE(gpu_test_reference(&request, &result));
        ++request.view.configuration_revision;
        GPU_REQUIRE(sixel_repair_gpu_scan(cache, &request, &result)
            == SIXEL_OK);
        GPU_REQUIRE(result.uploaded_bytes == GPU_TEST_COUNT * 26);
    } else if (mode == 163) {
        GPU_REQUIRE(sixel_repair_gpu_new(&other, NULL) == SIXEL_OK);
        memcpy(other_source, source, sizeof(source));
        other_source[3] = 4;
        other_request = request;
        other_request.view.source = other_source;
        GPU_REQUIRE(sixel_repair_gpu_scan(other, &other_request, &result)
            == SIXEL_OK);
        GPU_REQUIRE(result.summary.maximum > saved.summary.maximum);
        /* Equal revisions in two owners must not alias their resident data. */
        GPU_REQUIRE(sixel_repair_gpu_scan(cache, &request, &result)
            == SIXEL_OK);
        GPU_REQUIRE(result.uploaded_bytes == 0);
        GPU_REQUIRE(memcmp(&result.summary, &saved.summary,
            sizeof(result.summary)) == 0);
        GPU_REQUIRE(memcmp(result.samples, saved.samples,
            sizeof(result.samples)) == 0);
    } else if (mode == 164) {
        sixel_repair_gpu_evict(cache);
        request.policy = SIXEL_GPU_POLICY_OFF;
        GPU_REQUIRE(sixel_repair_gpu_scan(cache, &request, &result)
            == SIXEL_FALSE);
        GPU_REQUIRE(result.resident_bytes == 0);
        request.policy = SIXEL_GPU_POLICY_FORCE;
        GPU_REQUIRE(sixel_repair_gpu_scan(cache, &request, &result)
            == SIXEL_OK);
        GPU_REQUIRE(result.uploaded_bytes == GPU_TEST_COUNT * 26);
        GPU_REQUIRE(memcmp(&result.summary, &saved.summary,
            sizeof(result.summary)) == 0);
    } else if (mode == 165) {
        source[3] = FLT_MAX;
        ++request.view.source_generation;
        GPU_REQUIRE(sixel_repair_gpu_scan(cache, &request, &result)
            == SIXEL_BAD_INPUT);
        GPU_REQUIRE(memcmp(&result, &zero, sizeof(result)) == 0);
        request.policy = SIXEL_GPU_POLICY_AUTO;
        GPU_REQUIRE(sixel_repair_gpu_scan(cache, &request, &result)
            == SIXEL_FALSE);
        GPU_REQUIRE(memcmp(&result, &zero, sizeof(result)) == 0);
        source[3] = .125;
        ++request.view.source_generation;
        request.policy = SIXEL_GPU_POLICY_FORCE;
        GPU_REQUIRE(sixel_repair_gpu_scan(cache, &request, &result)
            == SIXEL_OK);
        GPU_REQUIRE(result.uploaded_bytes == GPU_TEST_COUNT * 26);
        GPU_REQUIRE(gpu_test_reference(&request, &result));
    } else if (mode == 166) {
        total = 0;
        request.sample_limit = 3;
        for (i = 0; i < GPU_TEST_COUNT; i += 129) {
            request.first = i;
            request.count = GPU_TEST_COUNT - i;
            if (request.count > 129) request.count = 129;
            GPU_REQUIRE(sixel_repair_gpu_scan(cache, &request, &result)
                == SIXEL_OK);
            GPU_REQUIRE(gpu_test_reference(&request, &result));
            GPU_REQUIRE(result.uploaded_bytes == 0);
            total += result.summary.eligible_count;
        }
        GPU_REQUIRE(total == saved.summary.eligible_count);
    }
    rc = 0;
end:
    gpu_test_fail_allocation = 0;
    sixel_repair_gpu_free(cache);
    sixel_repair_gpu_free(other);
    sixel_repair_gpu_free(failed);
    sixel_allocator_unref(allocator);
    return rc;
}
