/* SPDX-License-Identifier: MIT */
/* Four spatial phases, two cycles. k is a subpass, never video time.
 * This source is appended to the injected lookup program at preparation. */
struct Lso2GpuParams {
    uint width, height, ncolors, step;
    uint tiles_x, keycolor, delta, threshold;
    uint metric, columns, rows, tiled, selected_threshold;
    uint selection[128];
    uint batch_stage;
    uint variable;
    uint retained_stride, retained_x, retained_y;
};

/* The retained plane may be the canvas itself, so address it through the
 * output origin rather than by output index. */
static uint lso2_gpu_retained(uint i, constant Lso2GpuParams &p)
{
    return (i / p.width + p.retained_y) * p.retained_stride
        + i % p.width + p.retained_x;
}

static float3 lso2_gpu_color(constant uchar *palette, uint index)
{
    return float3(palette[3*index], palette[3*index+1], palette[3*index+2]);
}

static uint lso2_gpu_version(uint x, uint y, uint step,
    constant uchar *phase)
{
    uint bucket;

    bucket = phase[(y & 63)*64 + (x & 63)];
    return ((step / 4) + (bucket < step % 4 ? 1u : 0u)) & 1u;
}

static float3 lso2_gpu_send(device const float4 *a, device const float4 *b,
    constant uchar *phase, constant float *weights, int x, int y, uint tap,
    constant Lso2GpuParams &p)
{
    float3 error;
    uint3 row;
    uint i;

    if (x < 0 || y < 0 || x >= int(p.width) || y >= int(p.height))
        return float3(0.f);
    i = uint(y)*p.width + uint(x);
    error = lso2_gpu_version(uint(x), uint(y), p.step, phase) ? b[i].xyz
        : a[i].xyz;
    row = uint3(clamp(floor(abs(error) + .5f), 0.f, 255.f));
    return error * float3(weights[8*row.x+tap], weights[8*row.y+tap],
        weights[8*row.z+tap]);
}

#if LSO2_GPU_BATCH
#define LSO2_GPU_QUERY_BUFFERS \
    device float4 *queries [[buffer(16)]], \
    device const uint *indexes [[buffer(17)]],
#define LSO2_GPU_LOOKUP(q, i) indexes[i]
#else
#define LSO2_GPU_QUERY_BUFFERS
#define LSO2_GPU_LOOKUP(q, i) lookup_rgb8_fractional(q, palette, p.ncolors, \
    configuration, workspace)
#endif

kernel void lso2_gpu_initialize(device const uchar *source [[buffer(0)]],
    constant uchar *palette [[buffer(1)]],
    device float4 *a [[buffer(2)]],
    constant Lso2GpuParams &p [[buffer(4)]],
    device const uchar *transparent [[buffer(8)]],
    constant uchar *configuration [[buffer(12)]],
    device const uchar *workspace [[buffer(13)]],
    device atomic_uint *invalid [[buffer(14)]],
    LSO2_GPU_QUERY_BUFFERS
    uint i [[thread_position_in_grid]])
{
    float3 query;
    uint best;

    if (i >= p.width*p.height) return;
#if LSO2_GPU_BATCH
    if (p.batch_stage == 1) {
        queries[i] = transparent[i] ? float4(0.f) :
            float4(source[3*i], source[3*i+1], source[3*i+2], 1.f);
        return;
    }
#endif
    if (transparent[i]) { a[i] = float4(0.f); return; }
    query = float3(source[3*i], source[3*i+1], source[3*i+2]);
    best = LSO2_GPU_LOOKUP(query, i);
    if (best >= p.ncolors) {
        atomic_store_explicit(invalid, 1u, memory_order_relaxed);
        a[i] = float4(0.f);
        return;
    }
    a[i] = float4(query - lso2_gpu_color(palette, best), float(best));
}

kernel void lso2_gpu_iterate(device const uchar *source [[buffer(0)]],
    constant uchar *palette [[buffer(1)]],
    device float4 *a [[buffer(2)]], device float4 *b [[buffer(3)]],
    constant Lso2GpuParams &p [[buffer(4)]],
    constant uchar *phase [[buffer(5)]],
    constant uint *positions [[buffer(6)]],
    constant float *weights [[buffer(7)]],
    device const uchar *transparent [[buffer(8)]],
    constant uchar *configuration [[buffer(12)]],
    device const uchar *workspace [[buffer(13)]],
    device atomic_uint *invalid [[buffer(14)]],
    LSO2_GPU_QUERY_BUFFERS
    uint lane [[thread_position_in_grid]])
{
    uint tile, position, i, best;
    int x, y, direction, above;
    float3 incoming, query, previous;
    float4 updated, state;

#if LSO2_GPU_BATCH
    if (lane >= p.width*p.height) return;
    x = int(lane % p.width); y = int(lane / p.width);
    if (phase[(uint(y)&63)*64+(uint(x)&63)] != p.step%4) {
        if (p.batch_stage == 1) queries[lane] = float4(0.f);
        return;
    }
#else
    tile = lane / 1024;
    position = positions[(p.step % 4)*1024 + lane % 1024];
    x = int((tile % p.tiles_x)*64 + position % 64);
    y = int((tile / p.tiles_x)*64 + position / 64);
#endif
    if (x >= int(p.width) || y >= int(p.height)) return;
    i = uint(y)*p.width + uint(x);
    updated = float4(0.f);
    if (!transparent[i]) {
#if LSO2_GPU_BATCH
        if (p.batch_stage == 1) {
#endif
        direction = (y & 1) ? -1 : 1;
        above = ((y-1) & 1) ? -1 : 1;
        incoming = lso2_gpu_send(a,b,phase,weights,x-direction,y,0,p);
        incoming += lso2_gpu_send(a,b,phase,weights,x-4*direction,y,1,p);
        incoming += lso2_gpu_send(a,b,phase,weights,x+above,y-1,2,p);
        incoming += lso2_gpu_send(a,b,phase,weights,x,y-1,3,p);
        incoming += lso2_gpu_send(a,b,phase,weights,x-above,y-1,4,p);
        incoming += lso2_gpu_send(a,b,phase,weights,x,y-4,5,p);
        query = clamp(float3(source[3*i],source[3*i+1],source[3*i+2])
            + incoming, 0.f, 255.f);
        state = lso2_gpu_version(uint(x),uint(y),p.step,phase) ? b[i] : a[i];
        previous = state.xyz + lso2_gpu_color(palette,uint(state.w));
        query = previous + (query - previous);
#if LSO2_GPU_BATCH
            queries[i] = float4(query,1.f);
            return;
        }
        query = queries[i].xyz;
#endif
        best = LSO2_GPU_LOOKUP(query, i);
        if (best >= p.ncolors) {
            atomic_store_explicit(invalid, 1u, memory_order_relaxed);
        } else {
            updated = float4(query - lso2_gpu_color(palette,best),float(best));
        }
    }
#if LSO2_GPU_BATCH
    if (p.batch_stage == 1) { queries[i] = float4(0.f); return; }
#endif
    /* Earlier phases read new generations; this phase reads the old one.
     * No thread can read another same-phase thread's incomplete write. */
    if (((p.step / 4) + 1) & 1) b[i] = updated;
    else a[i] = updated;
}

/* The delta gate compares the same corrected query as ordinary refresh. */
static uint lso2_gpu_threshold(uint i, float3 delta,
    constant Lso2GpuParams &p, constant uint *levels)
{
    uint x, y, cell, selected, flags, spatial;

    if (p.columns == 0u) return p.threshold;
    x = i % p.width;
    y = i / p.width;
    cell = p.tiled != 0u ? (y & 63u) * 64u + (x & 63u)
        : (((y + 1u) * p.rows - 1u) / p.height) * p.columns
            + ((x + 1u) * p.columns - 1u) / p.width;
    if (p.variable != 0u && (p.tiled == 2u || p.tiled == 3u)) {
        selected = (levels[cell / 4u] >> (8u * (cell % 4u))) & 255u;
        if (selected == 255u) return p.threshold;
        flags = selected & 192u;
        selected &= 63u;
        if (flags != 0u) {
            spatial = (((y + 1u) * 64u - 1u) / p.height) * 64u
                + ((x + 1u) * 64u - 1u) / p.width;
            if ((p.selection[spatial / 32u] & (1u << (spatial % 32u))) != 0u) {
                if ((flags & 128u) != 0u) selected = 8u;
                if ((flags & 64u) != 0u && (p.tiled == 2u ||
                        abs(dot(delta, float3(54.0, 183.0, 19.0))) <= 1024.0))
                    selected = 16u;
            }
        }
        return min(p.threshold, selected);
    }
    if ((p.selection[cell / 32u] & (1u << (cell % 32u))) == 0u)
        return p.threshold;
    selected = p.variable != 0u
        ? (levels[cell / 4u] >> (8u * (cell % 4u))) & 255u
        : p.selected_threshold;
    return min(p.threshold, selected);
}

kernel void lso2_gpu_finish(constant uchar *palette [[buffer(1)]],
    device const float4 *a [[buffer(2)]],
    constant Lso2GpuParams &p [[buffer(4)]],
    device const uchar *transparent [[buffer(8)]],
    device const uchar *retained [[buffer(9)]],
    device const uchar *valid [[buffer(10)]],
    device uchar *result [[buffer(11)]],
    constant uchar *configuration [[buffer(12)]],
    device const uchar *workspace [[buffer(13)]],
    device atomic_uint *invalid [[buffer(14)]],
    device uchar *skip [[buffer(15)]],
    constant uint *levels [[buffer(18)]],
    LSO2_GPU_QUERY_BUFFERS
    uint i [[thread_position_in_grid]])
{
    uint best, code, threshold, r;
    float3 query, old, gap, fresh;
    float old_distance, new_distance;
    bool within;

    if (i >= p.width*p.height) return;
#if LSO2_GPU_BATCH
    if (p.batch_stage == 1) {
        queries[i] = transparent[i] ? float4(0.f) :
            float4(a[i].xyz+lso2_gpu_color(palette,uint(a[i].w)),1.f);
        return;
    }
#endif
    best = uint(a[i].w);
    code = 0;
    if (transparent[i]) {
        code = 1;
    } else if (p.delta && valid[lso2_gpu_retained(i, p)]) {
        /* Reconstruct the solved fractional query. Retained history is
         * consulted only now and never seeds a later frame's diffusion. */
        fresh = lso2_gpu_color(palette,best);
        query = a[i].xyz + fresh;
        best = LSO2_GPU_LOOKUP(query, i);
        if (best >= p.ncolors) {
            atomic_store_explicit(invalid, 1u, memory_order_relaxed);
            return;
        }
        fresh = lso2_gpu_color(palette,best);
        r = lso2_gpu_retained(i, p);
        old = float3(retained[3*r],retained[3*r+1],retained[3*r+2]);
        gap = abs(query-old);
        threshold = lso2_gpu_threshold(i, query - old, p, levels);
        within = all(gap <= float(threshold));
        if (p.metric == 2)
            within = dot(gap*gap,float3(2.f,4.f,1.f))
                <= 7.f*float(threshold)*float(threshold);
        old_distance = dot(query-old,query-old);
        new_distance = dot(query-fresh,query-fresh);
        if (within) code = any(gap != 0.f) ? 3 : 1;
        else if (old_distance <= new_distance) code = 1;
    }
    result[i] = uchar(code ? p.keycolor : best);
    skip[i] = uchar(code);
}

/* Preserve both the iterative index and the reconstructed-query choice.
 * The second state plane is dead after eight passes and owns the latter.
 * This kernel finishes the lookup lease before any history is acquired. */
kernel void lso2_gpu_solve_tail(constant uchar *palette [[buffer(1)]],
    device const float4 *a [[buffer(2)]],
    device float4 *b [[buffer(3)]],
    constant Lso2GpuParams &p [[buffer(4)]],
    device const uchar *transparent [[buffer(8)]],
    constant uchar *configuration [[buffer(12)]],
    device const uchar *workspace [[buffer(13)]],
    device atomic_uint *invalid [[buffer(14)]],
    LSO2_GPU_QUERY_BUFFERS
    uint i [[thread_position_in_grid]])
{
    float3 query;
    uint best;

    if (i >= p.width*p.height) return;
#if LSO2_GPU_BATCH
    if (p.batch_stage == 1) {
        queries[i] = transparent[i] ? float4(0.f) :
            float4(a[i].xyz+lso2_gpu_color(palette,uint(a[i].w)),1.f);
        return;
    }
#endif
    if (transparent[i]) return;
    query = a[i].xyz + lso2_gpu_color(palette,uint(a[i].w));
    best = LSO2_GPU_LOOKUP(query, i);
    if (best >= p.ncolors) {
        atomic_store_explicit(invalid, 1u, memory_order_relaxed);
        return;
    }
    b[i] = float4(query,float(best));
}

kernel void lso2_gpu_resolve(constant uchar *palette [[buffer(1)]],
    device const float4 *a [[buffer(2)]],
    device const float4 *b [[buffer(3)]],
    constant Lso2GpuParams &p [[buffer(4)]],
    device const uchar *transparent [[buffer(8)]],
    device const uchar *retained [[buffer(9)]],
    device const uchar *valid [[buffer(10)]],
    device uchar *result [[buffer(11)]],
    device uchar *skip [[buffer(15)]],
    constant uint *levels [[buffer(18)]],
    uint i [[thread_position_in_grid]])
{
    uint best, code, threshold, r;
    float3 query, old, gap, fresh;
    float old_distance, new_distance;
    bool within;

    if (i >= p.width*p.height) return;
    best = uint(a[i].w);
    code = 0;
    if (transparent[i]) {
        code = 1;
    } else if (p.delta && valid[lso2_gpu_retained(i, p)]) {
        /* The ready solution owns q and j; no lookup is needed here. */
        query = b[i].xyz;
        best = uint(b[i].w);
        fresh = lso2_gpu_color(palette,best);
        r = lso2_gpu_retained(i, p);
        old = float3(retained[3*r],retained[3*r+1],retained[3*r+2]);
        gap = abs(query-old);
        threshold = lso2_gpu_threshold(i, query - old, p, levels);
        within = all(gap <= float(threshold));
        if (p.metric == 2)
            within = dot(gap*gap,float3(2.f,4.f,1.f))
                <= 7.f*float(threshold)*float(threshold);
        old_distance = dot(query-old,query-old);
        new_distance = dot(query-fresh,query-fresh);
        if (within) code = any(gap != 0.f) ? 3 : 1;
        else if (old_distance <= new_distance) code = 1;
    }
    result[i] = uchar(code ? p.keycolor : best);
    skip[i] = uchar(code);
}
