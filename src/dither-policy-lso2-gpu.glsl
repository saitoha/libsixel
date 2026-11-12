/* SPDX-License-Identifier: MIT */
/*
 * LSO2 diffusion in four spatial phases and two cycles, the Vulkan port
 * of dither-policy-lso2-gpu.metal. k is a subpass, never video time. This
 * source follows the injected lookup policy. Six serpentine taps carry
 * the error with per-magnitude weights from the measured LSO2 ratios.
 *
 * KERNEL selects initialize, iterate, finish, solve_tail or resolve.
 */
/* Batch programs consume two spatial queries per lane. Point programs
 * retain their linear 64-thread dispatch. The host validates the shape. */
#if defined(SIXEL_LOOKUP_BATCH)
layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
#else
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
#endif

#if defined(SIXEL_LOOKUP_BATCH)
/* Private query slots survive the collective lookup. Stage one only
 * gathers queries; stage two consumes their indexes. No image-sized batch
 * scratch or extra dispatch is needed, and residual ownership stays here. */
vec4 batch_queries[2];
uvec2 batch_indexes;
uint batch_stage, batch_slot;
#endif

layout(constant_id = 0) const int KERNEL = 0;

const int LSO2_GPU_INITIALIZE = 0;
const int LSO2_GPU_ITERATE = 1;
const int LSO2_GPU_FINISH = 2;
const int LSO2_GPU_SOLVE_TAIL = 3;

layout(std430, set = 0, binding = 0) readonly buffer Source {
    uint8_t source[];
};
layout(std430, set = 0, binding = 2) buffer StateA {
    vec4 a[];
};
layout(std430, set = 0, binding = 3) buffer StateB {
    vec4 b[];
};
/* Field order and sizes match lso2_gpu_vk_params_t. */
layout(std430, set = 0, binding = 4) readonly buffer Params {
    uint width, height, ncolors, tiles_x;
    uint keycolor, delta, threshold, metric;
    uint columns, rows, tiled, selected_threshold;
    uint selection[128];
} p;
layout(std430, set = 0, binding = 5) readonly buffer Phase {
    uint8_t phase[];
};
layout(std430, set = 0, binding = 6) readonly buffer Positions {
    uint positions[];
};
/* 256 magnitude rows of eight weights; taps 0..5 are used. */
layout(std430, set = 0, binding = 7) readonly buffer Weights {
    float weights[];
};
layout(std430, set = 0, binding = 8) readonly buffer Transparent {
    uint8_t transparent[];
};
layout(std430, set = 0, binding = 9) readonly buffer Retained {
    uint8_t retained[];
};
layout(std430, set = 0, binding = 10) readonly buffer Valid {
    uint8_t valid[];
};
layout(std430, set = 0, binding = 11) writeonly buffer Result {
    uint8_t result[];
};
layout(std430, set = 0, binding = 14) buffer Invalid {
    uint invalid[];
};
layout(std430, set = 0, binding = 15) writeonly buffer Skip {
    uint8_t skip[];
};

/* step: subpass for iterate; count: invocations that do work; pitch:
 * invocations per dispatch row. */
layout(push_constant) uniform Push {
    uint step;
    uint pitch;
    uint count;
} pc;

vec3 lso2_gpu_color(uint index)
{
    return vec3(float(uint(lookup_palette[3u * index])),
        float(uint(lookup_palette[3u * index + 1u])),
        float(uint(lookup_palette[3u * index + 2u])));
}

vec3 lso2_gpu_source(uint i)
{
    return vec3(float(uint(source[3u * i])), float(uint(source[3u * i + 1u])),
        float(uint(source[3u * i + 2u])));
}

uint lso2_gpu_version(uint x, uint y)
{
    uint bucket;

    bucket = uint(phase[(y & 63u) * 64u + (x & 63u)]);
    return ((pc.step / 4u) + (bucket < pc.step % 4u ? 1u : 0u)) & 1u;
}

/* Each channel's error magnitude selects that channel's weight row. */
vec3 lso2_gpu_send(int x, int y, uint tap)
{
    vec3 error;
    uvec3 row;
    uint i;

    if (x < 0 || y < 0 || x >= int(p.width) || y >= int(p.height)) {
        return vec3(0.0);
    }
    i = uint(y) * p.width + uint(x);
    error = lso2_gpu_version(uint(x), uint(y)) != 0u ? b[i].xyz : a[i].xyz;
    row = uvec3(clamp(floor(abs(error) + 0.5), 0.0, 255.0));
    return error * vec3(weights[8u * row.x + tap],
        weights[8u * row.y + tap], weights[8u * row.z + tap]);
}

/* The solved state stores the residual against the chosen entry; rebuild
 * the fractional query the lookup consumed. */
vec3 lso2_gpu_solved_query(vec4 state)
{
    return state.xyz + lso2_gpu_color(uint(state.w));
}

uint lso2_gpu_classify(uint i, vec3 query, uint best)
{
    uint code, threshold, x, y, cell;
    vec3 old, gap, fresh;
    float old_distance, new_distance;
    bool within;

    code = 0u;
    fresh = lso2_gpu_color(best);
    old = vec3(float(uint(retained[3u * i])),
        float(uint(retained[3u * i + 1u])),
        float(uint(retained[3u * i + 2u])));
    gap = abs(query - old);
    threshold = p.threshold;
    if (p.columns != 0u) {
        x = i % p.width;
        y = i / p.width;
        cell = p.tiled != 0u ? (y & 63u) * 64u + (x & 63u)
            : (((y + 1u) * p.rows - 1u) / p.height) * p.columns
                + ((x + 1u) * p.columns - 1u) / p.width;
        if ((p.selection[cell / 32u] & (1u << (cell % 32u))) != 0u) {
            threshold = min(threshold, p.selected_threshold);
        }
    }
    within = all(lessThanEqual(gap, vec3(float(threshold))));
    if (p.metric == 2u) {
        within = dot(gap * gap, vec3(2.0, 4.0, 1.0))
            <= 7.0 * float(threshold) * float(threshold);
    }
    old_distance = dot(query - old, query - old);
    new_distance = dot(query - fresh, query - fresh);
    if (within) {
        code = any(notEqual(gap, vec3(0.0))) ? 3u : 1u;
    } else if (old_distance <= new_distance) {
        code = 1u;
    }
    return code;
}

void lso2_gpu_initialize(uint i)
{
    vec3 query;
    uint best;

    if (transparent[i] != uint8_t(0)) {
#if defined(SIXEL_LOOKUP_BATCH)
        if (batch_stage == 1u) return;
#endif
        a[i] = vec4(0.0);
        return;
    }
    query = lso2_gpu_source(i);
#if defined(SIXEL_LOOKUP_BATCH)
    if (batch_stage == 1u) {
        batch_queries[batch_slot] = vec4(query, 1.0);
        return;
    }
    best = batch_indexes[batch_slot];
#else
    best = lookup_rgb8_fractional(query, p.ncolors);
#endif
    if (best >= p.ncolors) {
        atomicOr(invalid[0], 1u);
        a[i] = vec4(0.0);
        return;
    }
    a[i] = vec4(query - lso2_gpu_color(best), float(best));
}

void lso2_gpu_iterate(uint lane)
{
    uint tile, position, i, best;
    int x, y, direction, above;
    vec3 incoming;
    precise vec3 query, previous;
    vec4 updated, state;

#if defined(SIXEL_LOOKUP_BATCH)
    x = int(lane % p.width);
    y = int(lane / p.width);
#else
    tile = lane / 1024u;
    position = positions[(pc.step % 4u) * 1024u + lane % 1024u];
    x = int((tile % p.tiles_x) * 64u + position % 64u);
    y = int((tile / p.tiles_x) * 64u + position / 64u);
#endif
    if (x >= int(p.width) || y >= int(p.height)) {
        return;
    }
    i = uint(y) * p.width + uint(x);
    updated = vec4(0.0);
    if (transparent[i] == uint8_t(0)) {
        direction = (y & 1) != 0 ? -1 : 1;
        above = ((y - 1) & 1) != 0 ? -1 : 1;
        incoming = lso2_gpu_send(x - direction, y, 0u);
        incoming += lso2_gpu_send(x - 4 * direction, y, 1u);
        incoming += lso2_gpu_send(x + above, y - 1, 2u);
        incoming += lso2_gpu_send(x, y - 1, 3u);
        incoming += lso2_gpu_send(x - above, y - 1, 4u);
        incoming += lso2_gpu_send(x, y - 4, 5u);
        query = clamp(lso2_gpu_source(i) + incoming, 0.0, 255.0);
        state = lso2_gpu_version(uint(x), uint(y)) != 0u ? b[i] : a[i];
        previous = state.xyz + lso2_gpu_color(uint(state.w));
        /* precise keeps the Metal rounding of this round trip. */
        query = previous + (query - previous);
#if defined(SIXEL_LOOKUP_BATCH)
        if (batch_stage == 1u) {
            batch_queries[batch_slot] = vec4(query, 1.0);
            return;
        }
        best = batch_indexes[batch_slot];
#else
        best = lookup_rgb8_fractional(query, p.ncolors);
#endif
        if (best >= p.ncolors) {
            atomicOr(invalid[0], 1u);
        } else {
            updated = vec4(query - lso2_gpu_color(best), float(best));
        }
    }
#if defined(SIXEL_LOOKUP_BATCH)
    if (batch_stage == 1u) return;
#endif
    /* Earlier phases read new generations; this phase reads the old one.
     * No invocation can read another same-phase invocation's write. */
    if ((((pc.step / 4u) + 1u) & 1u) != 0u) {
        b[i] = updated;
    } else {
        a[i] = updated;
    }
}

void lso2_gpu_finish(uint i)
{
    uint best, code;
    vec3 query;

    best = uint(a[i].w);
    code = 0u;
    if (transparent[i] != uint8_t(0)) {
        code = 1u;
    } else if (p.delta != 0u && valid[i] != uint8_t(0)) {
        /* Reconstruct the solved fractional query. Retained history is
         * consulted only now and never seeds a later frame's diffusion. */
        query = lso2_gpu_solved_query(a[i]);
#if defined(SIXEL_LOOKUP_BATCH)
        if (batch_stage == 1u) {
            batch_queries[batch_slot] = vec4(query, 1.0);
            return;
        }
        best = batch_indexes[batch_slot];
#else
        best = lookup_rgb8_fractional(query, p.ncolors);
#endif
        if (best >= p.ncolors) {
            atomicOr(invalid[0], 1u);
            return;
        }
        code = lso2_gpu_classify(i, query, best);
    }
#if defined(SIXEL_LOOKUP_BATCH)
    if (batch_stage == 1u) return;
#endif
    result[i] = uint8_t(code != 0u ? p.keycolor : best);
    skip[i] = uint8_t(code);
}

/* Preserve both the iterative index and the reconstructed-query choice.
 * The second state plane is dead after eight passes and owns the latter. */
void lso2_gpu_solve_tail(uint i)
{
    vec3 query;
    uint best;

    if (transparent[i] != uint8_t(0)) {
        return;
    }
    query = lso2_gpu_solved_query(a[i]);
#if defined(SIXEL_LOOKUP_BATCH)
    if (batch_stage == 1u) {
        batch_queries[batch_slot] = vec4(query, 1.0);
        return;
    }
    best = batch_indexes[batch_slot];
#else
    best = lookup_rgb8_fractional(query, p.ncolors);
#endif
    if (best >= p.ncolors) {
        atomicOr(invalid[0], 1u);
        return;
    }
    b[i] = vec4(query, float(best));
}

void lso2_gpu_resolve(uint i)
{
    uint best, code;

    best = uint(a[i].w);
    code = 0u;
    if (transparent[i] != uint8_t(0)) {
        code = 1u;
    } else if (p.delta != 0u && valid[i] != uint8_t(0)) {
        /* The ready solution owns q and j; no lookup is needed here. */
        best = uint(b[i].w);
        code = lso2_gpu_classify(i, b[i].xyz, best);
    }
    result[i] = uint8_t(code != 0u ? p.keycolor : best);
    skip[i] = uint8_t(code);
}

void lso2_gpu_run(uint index)
{
    if (KERNEL == LSO2_GPU_INITIALIZE) {
        lso2_gpu_initialize(index);
    } else if (KERNEL == LSO2_GPU_ITERATE) {
        lso2_gpu_iterate(index);
    } else if (KERNEL == LSO2_GPU_FINISH) {
        lso2_gpu_finish(index);
    } else if (KERNEL == LSO2_GPU_SOLVE_TAIL) {
        lso2_gpu_solve_tail(index);
    } else {
        lso2_gpu_resolve(index);
    }
}

void main()
{
#if defined(SIXEL_LOOKUP_BATCH)
    uint tile, columns, pos, h, x, y, index[2];
    bool enabled[2];

    columns = (p.width + 7u) / 8u;
    tile = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
    for (h = 0u; h < 2u; ++h) {
        pos = gl_LocalInvocationID.x + h * 32u;
        x = (tile % columns) * 8u + pos % 8u;
        y = (tile / columns) * 8u + pos / 8u;
        index[h] = y * p.width + x;
        enabled[h] = x < p.width && y < p.height;
        if (enabled[h] && KERNEL == LSO2_GPU_ITERATE) {
            enabled[h] = uint(phase[(y & 63u) * 64u + (x & 63u)])
                == pc.step % 4u;
        }
        batch_queries[h] = vec4(0.0);
    }
    /* Deferred resolve consumes the frozen solution without any lookup. */
    if (KERNEL > LSO2_GPU_SOLVE_TAIL) {
        for (h = 0u; h < 2u; ++h) {
            if (enabled[h]) lso2_gpu_resolve(index[h]);
        }
        return;
    }
    batch_stage = 1u;
    for (h = 0u; h < 2u; ++h) {
        batch_slot = h;
        if (enabled[h]) lso2_gpu_run(index[h]);
    }
    lookup_rgb8_batch(batch_queries, p.ncolors, batch_indexes);
    batch_stage = 2u;
    for (h = 0u; h < 2u; ++h) {
        batch_slot = h;
        if (enabled[h]) lso2_gpu_run(index[h]);
    }
#else
    uint index;

    index = gl_GlobalInvocationID.y * pc.pitch + gl_GlobalInvocationID.x;
    if (index < pc.count) lso2_gpu_run(index);
#endif
}
