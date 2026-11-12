/* SPDX-License-Identifier: MIT */
/*
 * Jarvis, Judice and Ninke diffusion in four spatial phases and two cycles, the
 * Vulkan port of dither-policy-jajuni-gpu.metal. k is a subpass, never video
 * time. This source follows the injected lookup policy. Each tap names the
 * sender of an error contribution: dy rows above, and dx columns behind it
 * along the sender row's scan direction. The carry follows the CPU kernel:
 * deadzone, soft or hard limiting and integer rounding of the corrected
 * sample.
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

const int JAJUNI_GPU_INITIALIZE = 0;
const int JAJUNI_GPU_ITERATE = 1;
const int JAJUNI_GPU_FINISH = 2;
const int JAJUNI_GPU_SOLVE_TAIL = 3;

layout(std430, set = 0, binding = 0) readonly buffer Source {
    uint8_t source[];
};
layout(std430, set = 0, binding = 2) buffer StateA {
    vec4 a[];
};
layout(std430, set = 0, binding = 3) buffer StateB {
    vec4 b[];
};
/* Field order and sizes match jajuni_gpu_vk_params_t. */
layout(std430, set = 0, binding = 4) readonly buffer Params {
    uint width, height, ncolors, tiles_x;
    uint keycolor, delta, threshold, metric;
    uint columns, rows, tiled, selected_threshold;
    uint selection[128];
    uint serpentine, clamp_hard;
    float deadzone, clamp_lower, clamp_upper;
} p;
layout(std430, set = 0, binding = 5) readonly buffer Phase {
    uint8_t phase[];
};
layout(std430, set = 0, binding = 6) readonly buffer Positions {
    uint positions[];
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

const int jajuni_gpu_dx[12] = int[12](1, 2, -2, -1, 0, 1, 2, -2, -1, 0, 1, 2);
const int jajuni_gpu_dy[12] = int[12](0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2);
/* Weights over the kernel denominator 48. */
const float jajuni_gpu_weights[12] = float[12](
    7.0 / 48.0,
    5.0 / 48.0,
    3.0 / 48.0,
    5.0 / 48.0,
    7.0 / 48.0,
    5.0 / 48.0,
    3.0 / 48.0,
    1.0 / 48.0,
    3.0 / 48.0,
    5.0 / 48.0,
    3.0 / 48.0,
    1.0 / 48.0);

vec3 jajuni_gpu_color(uint index)
{
    return vec3(float(uint(lookup_palette[3u * index])),
        float(uint(lookup_palette[3u * index + 1u])),
        float(uint(lookup_palette[3u * index + 2u])));
}

vec3 jajuni_gpu_source(uint i)
{
    return vec3(float(uint(source[3u * i])), float(uint(source[3u * i + 1u])),
        float(uint(source[3u * i + 2u])));
}

uint jajuni_gpu_version(uint x, uint y)
{
    uint bucket;

    bucket = uint(phase[(y & 63u) * 64u + (x & 63u)]);
    return ((pc.step / 4u) + (bucket < pc.step % 4u ? 1u : 0u)) & 1u;
}

int jajuni_gpu_direction(int y)
{
    return p.serpentine != 0u && (y & 1) != 0 ? -1 : 1;
}

/* Limit the corrected sample as the CPU carry limiter does: scale the whole
 * correction until every channel lies within the soft bounds, preserving
 * its direction; hard clipping limits each channel independently. */
vec3 jajuni_gpu_carry(vec3 value_source, vec3 incoming)
{
    vec3 value;
    float scale;
    int n;

    if (dot(incoming, incoming) < p.deadzone * p.deadzone) {
        return value_source;
    }
    value = value_source + incoming;
    if (p.clamp_hard != 0u) {
        return roundEven(clamp(value, 0.0, 255.0));
    }
    scale = 1.0;
    for (n = 0; n < 3; ++n) {
        if (value[n] < p.clamp_lower) {
            scale = min(scale,
                (value_source[n] - p.clamp_lower) / (-incoming[n]));
        } else if (value[n] > p.clamp_upper) {
            scale = min(scale,
                (p.clamp_upper - value_source[n]) / incoming[n]);
        }
    }
    return roundEven(value_source + incoming * scale);
}

vec3 jajuni_gpu_send(int x, int y, uint tap)
{
    uint i;

    if (x < 0 || y < 0 || x >= int(p.width) || y >= int(p.height)) {
        return vec3(0.0);
    }
    i = uint(y) * p.width + uint(x);
    return (jajuni_gpu_version(uint(x), uint(y)) != 0u ? b[i].xyz
        : a[i].xyz) * jajuni_gpu_weights[tap];
}

vec3 jajuni_gpu_candidate(int x, int y)
{
    vec3 incoming;
    uint i, tap;
    int sender;

    i = uint(y) * p.width + uint(x);
    incoming = vec3(0.0);
    for (tap = 0u; tap < 12u; ++tap) {
        sender = y - jajuni_gpu_dy[tap];
        incoming += jajuni_gpu_send(
            x - jajuni_gpu_direction(sender) * jajuni_gpu_dx[tap], sender,
            tap);
    }
    return jajuni_gpu_carry(jajuni_gpu_source(i), incoming);
}

/* The solved state stores the residual against the chosen entry; rebuild
 * the fractional query the lookup consumed. The carry residual is
 * unclamped, so the query is clamped to the byte range again. */
vec3 jajuni_gpu_solved_query(vec4 state)
{
    return clamp(state.xyz + jajuni_gpu_color(uint(state.w)), 0.0, 255.0);
}

uint jajuni_gpu_classify(uint i, vec3 query, uint best)
{
    uint code, threshold, x, y, cell;
    vec3 old, gap, fresh;
    float old_distance, new_distance;
    bool within;

    code = 0u;
    fresh = jajuni_gpu_color(best);
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

void jajuni_gpu_initialize(uint i)
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
    query = jajuni_gpu_source(i);
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
    a[i] = vec4(query - jajuni_gpu_color(best), float(best));
}

void jajuni_gpu_iterate(uint lane)
{
    uint tile, position, i, best;
    int x, y;
    vec3 candidate, query;
    vec4 updated;

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
        candidate = jajuni_gpu_candidate(x, y);
        query = clamp(candidate, 0.0, 255.0);
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
            updated = vec4(candidate - jajuni_gpu_color(best), float(best));
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

void jajuni_gpu_finish(uint i)
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
        query = jajuni_gpu_solved_query(a[i]);
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
        code = jajuni_gpu_classify(i, query, best);
    }
#if defined(SIXEL_LOOKUP_BATCH)
    if (batch_stage == 1u) return;
#endif
    result[i] = uint8_t(code != 0u ? p.keycolor : best);
    skip[i] = uint8_t(code);
}

/* Preserve both the iterative index and the reconstructed-query choice.
 * The second state plane is dead after eight passes and owns the latter. */
void jajuni_gpu_solve_tail(uint i)
{
    vec3 query;
    uint best;

    if (transparent[i] != uint8_t(0)) {
        return;
    }
    query = jajuni_gpu_solved_query(a[i]);
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

void jajuni_gpu_resolve(uint i)
{
    uint best, code;

    best = uint(a[i].w);
    code = 0u;
    if (transparent[i] != uint8_t(0)) {
        code = 1u;
    } else if (p.delta != 0u && valid[i] != uint8_t(0)) {
        /* The ready solution owns q and j; no lookup is needed here. */
        best = uint(b[i].w);
        code = jajuni_gpu_classify(i, b[i].xyz, best);
    }
    result[i] = uint8_t(code != 0u ? p.keycolor : best);
    skip[i] = uint8_t(code);
}

void jajuni_gpu_run(uint index)
{
    if (KERNEL == JAJUNI_GPU_INITIALIZE) {
        jajuni_gpu_initialize(index);
    } else if (KERNEL == JAJUNI_GPU_ITERATE) {
        jajuni_gpu_iterate(index);
    } else if (KERNEL == JAJUNI_GPU_FINISH) {
        jajuni_gpu_finish(index);
    } else if (KERNEL == JAJUNI_GPU_SOLVE_TAIL) {
        jajuni_gpu_solve_tail(index);
    } else {
        jajuni_gpu_resolve(index);
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
        if (enabled[h] && KERNEL == JAJUNI_GPU_ITERATE) {
            enabled[h] = uint(phase[(y & 63u) * 64u + (x & 63u)])
                == pc.step % 4u;
        }
        batch_queries[h] = vec4(0.0);
    }
    /* Deferred resolve consumes the frozen solution without any lookup. */
    if (KERNEL > JAJUNI_GPU_SOLVE_TAIL) {
        for (h = 0u; h < 2u; ++h) {
            if (enabled[h]) jajuni_gpu_resolve(index[h]);
        }
        return;
    }
    batch_stage = 1u;
    for (h = 0u; h < 2u; ++h) {
        batch_slot = h;
        if (enabled[h]) jajuni_gpu_run(index[h]);
    }
    lookup_rgb8_batch(batch_queries, p.ncolors, batch_indexes);
    batch_stage = 2u;
    for (h = 0u; h < 2u; ++h) {
        batch_slot = h;
        if (enabled[h]) jajuni_gpu_run(index[h]);
    }
#else
    uint index;

    index = gl_GlobalInvocationID.y * pc.pitch + gl_GlobalInvocationID.x;
    if (index < pc.count) jajuni_gpu_run(index);
#endif
}
