/* SPDX-License-Identifier: MIT */
/* Stored float coordinates define the independent double reference. The
 * normalized metric is specified here, without the production evaluator. */
static int
filtered_oracle(float const *palette, int n, float const *q)
{
    double best, distance, delta;
    int i, c, winner;

    best = DBL_MAX;
    winner = 0;
    for (i = 0; i < n; ++i) {
        distance = 0;
        for (c = 0; c < 3; ++c) {
            delta = (double)q[c] - palette[i * 3 + c];
            distance += delta * delta;
        }
        if (distance < best) {
            best = distance;
            winner = i;
        }
    }
    return winner;
}

static int filtered_allocations;
static int filtered_fail_at;

static void *
filtered_malloc(size_t size)
{
    ++filtered_allocations;
    return filtered_allocations == filtered_fail_at ? NULL : malloc(size);
}

int
FILTER_TEST(int argc, char **argv)
{
    static int const sizes[] = {15, 16, 17, 31, 32, 33, 255, 256};
    sixel_allocator_t *allocator;
    sixel_lookup_policy_interface_t *policy;
    sixel_lookup_policy_prepare_request_t request;
    FILTER_OBJECT *object;
    unsigned char bytes[256 * 3] = {0};
    float palette[256 * 3], q[3], samples[9];
    sixel_index_t indexes[3];
    float scale;
    int mode, ok, k, n, i, c, pass, expect, failure;

    allocator = NULL;
    policy = NULL;
    ok = 0;
    k = pass = 0;
    mode = argc > 1 ? atoi(argv[1]) : 0;
    filtered_allocations = filtered_fail_at = 0;
    if (sixel_allocator_new(&allocator, filtered_malloc, NULL, NULL, NULL)
            != SIXEL_OK || FILTER_NEW(allocator, (void **)&policy)
            != SIXEL_OK) goto end;
    object = (FILTER_OBJECT *)policy;
    memset(&request, 0, sizeof(request));
    request.allocator = allocator;
    request.palette = bytes;
    request.palette_float = palette;
    request.depth = 3;
    request.float_depth = 3 * (int)sizeof(float);
    request.pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    for (k = 0; k < 8; ++k) {
        n = sizes[k];
        request.reqcolor = n;
        for (pass = 0; pass < 4; ++pass) {
            scale = mode == 1 ? ldexpf(1, -60 + pass)
                : mode == 2 ? ldexpf(1, 38 + pass) : 1;
            for (i = 0; i < n * 3; ++i) palette[i] = 4 * scale;
            /* The first and last candidates have the same float score at
             * unit scale, but double must select the last (tail) entry. */
            palette[0] = palette[(n - 1) * 3] = scale;
            palette[1] = ldexpf(scale, -12);
            palette[2] = palette[(n - 1) * 3 + 1] =
                palette[(n - 1) * 3 + 2] = 0;
            memset(q, 0, sizeof(q));
            if (pass & 1) {
                /* An exact tie across lanes must choose the first slot. */
                palette[1] = 0;
            }
            if (mode == 2) {
                for (i = 0; i < n * 3; ++i) palette[i] *= .25f;
                palette[(n - 1) * 3] = pass & 1
                    ? nextafterf(0x1p40f, INFINITY) : 0x1p40f;
                q[0] = -palette[(n - 1) * 3];
            }
            /* Optional double and float planar allocations are the second
             * and third allocations of prepare; failure must stay exact. */
            failure = mode == 3 ? 2 + pass % 2 : 0;
            filtered_allocations = 0;
            filtered_fail_at = failure;
            if (policy->vtbl->prepare(policy, &request) != SIXEL_OK)
                goto end;
            filtered_fail_at = 0;
            if (mode == 3) {
                if (object->state_float.planar32 != NULL ||
                        (failure == 2 && object->state_float.planar != NULL))
                    goto end;
            } else if (mode == 2 && (pass & 1)) {
                if (object->state_float.planar32 != NULL) goto end;
            } else if (object->state_float.planar32 == NULL) {
                goto end;
            }
            for (i = 0; i < 3; ++i) {
                for (c = 0; c < 3; ++c) samples[i * 3 + c] = q[c];
                samples[i * 3 + 1] = i == 1
                    ? nextafterf(q[1], INFINITY) : q[1];
            }
            policy->vtbl->map_pixels(policy,
                (unsigned char const *)samples, 3, 3 * sizeof(float),
                indexes, 1, NULL);
            for (i = 0; i < 3; ++i) {
                expect = filtered_oracle(palette, n, samples + i * 3);
                if (indexes[i] != expect || policy->vtbl->map_pixel(policy,
                    (unsigned char const *)(samples + i * 3)) != expect) {
                    fprintf(stderr, "filtered mode=%d n=%d pass=%d\n",
                        mode, n, pass);
                    goto end;
                }
            }
        }
    }
    ok = 1;
end:
    filtered_fail_at = 0;
    if (policy != NULL) policy->vtbl->unref(policy);
    sixel_allocator_unref(allocator);
    if (!ok) {
        fprintf(stderr, "filtered prepare mode=%d size=%d pass=%d\n",
            mode, k, pass);
    }
    return ok ? 0 : 1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
