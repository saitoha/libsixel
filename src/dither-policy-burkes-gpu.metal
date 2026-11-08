/* SPDX-License-Identifier: MIT */
/* Burkes diffusion in four spatial phases and two cycles. k is a subpass,
 * never video time. This source is appended to the injected lookup program
 * at preparation. Each tap names the sender of an error contribution: dy
 * rows above, and dx columns behind it along the sender row's scan
 * direction. The carry follows the CPU kernel: deadzone, soft or hard
 * limiting and integer rounding of the corrected sample. */
struct BurkesGpuParams {
    uint width, height, ncolors, step;
    uint tiles_x, keycolor, delta, threshold;
    uint metric, columns, rows, tiled, selected_threshold;
    uint selection[128];
    uint batch_stage;
    uint serpentine, clamp_hard;
    float deadzone, clamp_lower, clamp_upper;
};

constant int burkes_gpu_dx[7] = { 1, 2, -2, -1, 0, 1, 2 };
constant int burkes_gpu_dy[7] = { 0, 0, 1, 1, 1, 1, 1 };
/* Weights over the kernel denominator 32. */
constant float burkes_gpu_weights[7] = {
    8.f/32.f,
    4.f/32.f,
    2.f/32.f,
    4.f/32.f,
    8.f/32.f,
    4.f/32.f,
    2.f/32.f
};

static int burkes_gpu_direction(int y, constant BurkesGpuParams &p)
{
    return p.serpentine && (y & 1) ? -1 : 1;
}

/* Limit the corrected sample as the CPU carry limiter does: scale the whole
 * correction until every channel lies within the soft bounds, preserving
 * its direction; hard clipping limits each channel independently. */
static float3 burkes_gpu_carry(float3 source, float3 incoming,
    constant BurkesGpuParams &p)
{
    float3 value;
    float scale;
    uint n;

    if (dot(incoming,incoming) < p.deadzone*p.deadzone) return source;
    value = source + incoming;
    if (p.clamp_hard) return rint(clamp(value, 0.f, 255.f));
    scale = 1.f;
    for (n = 0; n < 3; ++n) {
        if (value[n] < p.clamp_lower)
            scale = min(scale, (source[n]-p.clamp_lower)/(-incoming[n]));
        else if (value[n] > p.clamp_upper)
            scale = min(scale, (p.clamp_upper-source[n])/incoming[n]);
    }
    return rint(source + incoming*scale);
}

static float3 burkes_gpu_color(constant uchar *palette, uint index)
{
    return float3(palette[3*index], palette[3*index+1], palette[3*index+2]);
}

static uint burkes_gpu_version(uint x, uint y, uint step,
    constant uchar *phase)
{
    uint bucket;

    bucket = phase[(y & 63)*64 + (x & 63)];
    return ((step / 4) + (bucket < step % 4 ? 1u : 0u)) & 1u;
}

static float3 burkes_gpu_send(device const float4 *a, device const float4 *b,
    constant uchar *phase, int x, int y, uint tap,
    constant BurkesGpuParams &p)
{
    uint i;

    if (x < 0 || y < 0 || x >= int(p.width) || y >= int(p.height))
        return float3(0.f);
    i = uint(y)*p.width + uint(x);
    return (burkes_gpu_version(uint(x), uint(y), p.step, phase) ? b[i].xyz
        : a[i].xyz) * burkes_gpu_weights[tap];
}

/* The rounded candidate carries the residual; lookup uses its byte
 * projection, as in the CPU carry path. */
static float3 burkes_gpu_candidate(device const uchar *source,
    device const float4 *a, device const float4 *b, constant uchar *phase,
    int x, int y, constant BurkesGpuParams &p)
{
    float3 incoming;
    uint i, tap;
    int sender;

    i = uint(y)*p.width + uint(x);
    incoming = float3(0.f);
    for (tap = 0; tap < 7u; ++tap) {
        sender = y - burkes_gpu_dy[tap];
        incoming += burkes_gpu_send(a,b,phase,
            x - burkes_gpu_direction(sender,p)*burkes_gpu_dx[tap],sender,tap,p);
    }
    return burkes_gpu_carry(float3(source[3*i],source[3*i+1],source[3*i+2]),
        incoming, p);
}

#if BURKES_GPU_BATCH
#define BURKES_GPU_QUERY_BUFFERS \
    device float4 *queries [[buffer(16)]], \
    device const uint *indexes [[buffer(17)]],
#define BURKES_GPU_LOOKUP(q, i) indexes[i]
#else
#define BURKES_GPU_QUERY_BUFFERS
#define BURKES_GPU_LOOKUP(q, i) lookup_rgb8_fractional(q, palette, p.ncolors, \
    configuration, workspace)
#endif

kernel void burkes_gpu_initialize(device const uchar *source [[buffer(0)]],
    constant uchar *palette [[buffer(1)]],
    device float4 *a [[buffer(2)]],
    constant BurkesGpuParams &p [[buffer(4)]],
    device const uchar *transparent [[buffer(8)]],
    constant uchar *configuration [[buffer(12)]],
    device const uchar *workspace [[buffer(13)]],
    device atomic_uint *invalid [[buffer(14)]],
    BURKES_GPU_QUERY_BUFFERS
    uint i [[thread_position_in_grid]])
{
    float3 query;
    uint best;

    if (i >= p.width*p.height) return;
#if BURKES_GPU_BATCH
    if (p.batch_stage == 1) {
        queries[i] = transparent[i] ? float4(0.f) :
            float4(source[3*i], source[3*i+1], source[3*i+2], 1.f);
        return;
    }
#endif
    if (transparent[i]) { a[i] = float4(0.f); return; }
    query = float3(source[3*i], source[3*i+1], source[3*i+2]);
    best = BURKES_GPU_LOOKUP(query, i);
    if (best >= p.ncolors) {
        atomic_store_explicit(invalid, 1u, memory_order_relaxed);
        a[i] = float4(0.f);
        return;
    }
    a[i] = float4(query - burkes_gpu_color(palette, best), float(best));
}

kernel void burkes_gpu_iterate(device const uchar *source [[buffer(0)]],
    constant uchar *palette [[buffer(1)]],
    device float4 *a [[buffer(2)]], device float4 *b [[buffer(3)]],
    constant BurkesGpuParams &p [[buffer(4)]],
    constant uchar *phase [[buffer(5)]],
    constant uint *positions [[buffer(6)]],
    device const uchar *transparent [[buffer(8)]],
    constant uchar *configuration [[buffer(12)]],
    device const uchar *workspace [[buffer(13)]],
    device atomic_uint *invalid [[buffer(14)]],
    BURKES_GPU_QUERY_BUFFERS
    uint lane [[thread_position_in_grid]])
{
    uint tile, position, i, best;
    int x, y;
    float3 query, candidate;
    float4 updated;

#if BURKES_GPU_BATCH
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
        /* Both batch stages read the same unchanged generations, so the
         * second stage recomputes the candidate the first one queried. */
        candidate = burkes_gpu_candidate(source, a, b, phase, x, y, p);
        query = clamp(candidate, 0.f, 255.f);
#if BURKES_GPU_BATCH
        if (p.batch_stage == 1) {
            queries[i] = float4(query,1.f);
            return;
        }
#endif
        best = BURKES_GPU_LOOKUP(query, i);
        if (best >= p.ncolors) {
            atomic_store_explicit(invalid, 1u, memory_order_relaxed);
        } else {
            updated = float4(candidate - burkes_gpu_color(palette,best),
                float(best));
        }
    }
#if BURKES_GPU_BATCH
    if (p.batch_stage == 1) { queries[i] = float4(0.f); return; }
#endif
    /* Earlier phases read new generations; this phase reads the old one.
     * No thread can read another same-phase thread's incomplete write. */
    if (((p.step / 4) + 1) & 1) b[i] = updated;
    else a[i] = updated;
}

kernel void burkes_gpu_finish(constant uchar *palette [[buffer(1)]],
    device const float4 *a [[buffer(2)]],
    constant BurkesGpuParams &p [[buffer(4)]],
    device const uchar *transparent [[buffer(8)]],
    device const uchar *retained [[buffer(9)]],
    device const uchar *valid [[buffer(10)]],
    device uchar *result [[buffer(11)]],
    constant uchar *configuration [[buffer(12)]],
    device const uchar *workspace [[buffer(13)]],
    device atomic_uint *invalid [[buffer(14)]],
    device uchar *skip [[buffer(15)]],
    BURKES_GPU_QUERY_BUFFERS
    uint i [[thread_position_in_grid]])
{
    uint best, code, threshold, x, y, cell;
    float3 query, old, gap, fresh;
    float old_distance, new_distance;
    bool within;

    if (i >= p.width*p.height) return;
#if BURKES_GPU_BATCH
    if (p.batch_stage == 1) {
        queries[i] = transparent[i] ? float4(0.f) :
            float4(clamp(a[i].xyz+burkes_gpu_color(palette,uint(a[i].w)),
                0.f, 255.f),1.f);
        return;
    }
#endif
    best = uint(a[i].w);
    code = 0;
    if (transparent[i]) {
        code = 1;
    } else if (p.delta && valid[i]) {
        /* Reconstruct the solved fractional query. Retained history is
         * consulted only now and never seeds a later frame's diffusion. */
        fresh = burkes_gpu_color(palette,best);
        query = clamp(a[i].xyz + fresh, 0.f, 255.f);
        best = BURKES_GPU_LOOKUP(query, i);
        if (best >= p.ncolors) {
            atomic_store_explicit(invalid, 1u, memory_order_relaxed);
            return;
        }
        fresh = burkes_gpu_color(palette,best);
        old = float3(retained[3*i],retained[3*i+1],retained[3*i+2]);
        gap = abs(query-old);
        threshold = p.threshold;
        if (p.columns) {
            x = i % p.width; y = i / p.width;
            cell = p.tiled ? (y & 63)*64 + (x & 63)
                : (((y+1)*p.rows-1)/p.height)*p.columns
                    + ((x+1)*p.columns-1)/p.width;
            if (p.selection[cell/32] & (1u << (cell%32)))
                threshold = min(threshold,p.selected_threshold);
        }
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
kernel void burkes_gpu_solve_tail(constant uchar *palette [[buffer(1)]],
    device const float4 *a [[buffer(2)]],
    device float4 *b [[buffer(3)]],
    constant BurkesGpuParams &p [[buffer(4)]],
    device const uchar *transparent [[buffer(8)]],
    constant uchar *configuration [[buffer(12)]],
    device const uchar *workspace [[buffer(13)]],
    device atomic_uint *invalid [[buffer(14)]],
    BURKES_GPU_QUERY_BUFFERS
    uint i [[thread_position_in_grid]])
{
    float3 query;
    uint best;

    if (i >= p.width*p.height) return;
#if BURKES_GPU_BATCH
    if (p.batch_stage == 1) {
        queries[i] = transparent[i] ? float4(0.f) :
            float4(clamp(a[i].xyz+burkes_gpu_color(palette,uint(a[i].w)),
                0.f, 255.f),1.f);
        return;
    }
#endif
    if (transparent[i]) return;
    query = clamp(a[i].xyz + burkes_gpu_color(palette,uint(a[i].w)),
        0.f, 255.f);
    best = BURKES_GPU_LOOKUP(query, i);
    if (best >= p.ncolors) {
        atomic_store_explicit(invalid, 1u, memory_order_relaxed);
        return;
    }
    b[i] = float4(query,float(best));
}

kernel void burkes_gpu_resolve(constant uchar *palette [[buffer(1)]],
    device const float4 *a [[buffer(2)]],
    device const float4 *b [[buffer(3)]],
    constant BurkesGpuParams &p [[buffer(4)]],
    device const uchar *transparent [[buffer(8)]],
    device const uchar *retained [[buffer(9)]],
    device const uchar *valid [[buffer(10)]],
    device uchar *result [[buffer(11)]],
    device uchar *skip [[buffer(15)]],
    uint i [[thread_position_in_grid]])
{
    uint best, code, threshold, x, y, cell;
    float3 query, old, gap, fresh;
    float old_distance, new_distance;
    bool within;

    if (i >= p.width*p.height) return;
    best = uint(a[i].w);
    code = 0;
    if (transparent[i]) {
        code = 1;
    } else if (p.delta && valid[i]) {
        /* The ready solution owns q and j; no lookup is needed here. */
        query = b[i].xyz;
        best = uint(b[i].w);
        fresh = burkes_gpu_color(palette,best);
        old = float3(retained[3*i],retained[3*i+1],retained[3*i+2]);
        gap = abs(query-old);
        threshold = p.threshold;
        if (p.columns) {
            x = i % p.width; y = i / p.width;
            cell = p.tiled ? (y & 63)*64 + (x & 63)
                : (((y+1)*p.rows-1)/p.height)*p.columns
                    + ((x+1)*p.columns-1)/p.width;
            if (p.selection[cell/32] & (1u << (cell%32)))
                threshold = min(threshold,p.selected_threshold);
        }
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
