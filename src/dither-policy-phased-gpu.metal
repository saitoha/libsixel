/* SPDX-License-Identifier: MIT */
/* Four spatial phases, two cycles. k is a subpass, never video time.
 * This source is appended to the injected lookup program at preparation. */
struct PhasedParams {
    uint width, height, ncolors, step;
    uint tiles_x, keycolor, delta, threshold;
    uint metric, columns, rows, tiled, selected_threshold;
    uint selection[128];
};

static float3 phased_color(constant uchar *palette, uint index)
{
    return float3(palette[3*index], palette[3*index+1], palette[3*index+2]);
}

static uint phased_version(uint x, uint y, uint step,
    constant uchar *phase)
{
    uint bucket;

    bucket = phase[(y & 63)*64 + (x & 63)];
    return ((step / 4) + (bucket < step % 4 ? 1u : 0u)) & 1u;
}

static float3 phased_send(device const float4 *a, device const float4 *b,
    constant uchar *phase, constant float *weights, int x, int y, uint tap,
    constant PhasedParams &p)
{
    float3 error;
    uint3 row;
    uint i;

    if (x < 0 || y < 0 || x >= int(p.width) || y >= int(p.height))
        return float3(0.f);
    i = uint(y)*p.width + uint(x);
    error = phased_version(uint(x), uint(y), p.step, phase) ? b[i].xyz
        : a[i].xyz;
    row = uint3(clamp(floor(abs(error) + .5f), 0.f, 255.f));
    return error * float3(weights[8*row.x+tap], weights[8*row.y+tap],
        weights[8*row.z+tap]);
}

kernel void phased_initialize(device const uchar *source [[buffer(0)]],
    constant uchar *palette [[buffer(1)]],
    device float4 *a [[buffer(2)]],
    constant PhasedParams &p [[buffer(4)]],
    device const uchar *transparent [[buffer(8)]],
    constant uchar *configuration [[buffer(12)]],
    device const uchar *workspace [[buffer(13)]],
    device atomic_uint *invalid [[buffer(14)]],
    uint i [[thread_position_in_grid]])
{
    float3 query;
    uint best;

    if (i >= p.width*p.height) return;
    if (transparent[i]) { a[i] = float4(0.f); return; }
    query = float3(source[3*i], source[3*i+1], source[3*i+2]);
    best = lookup_rgb8_fractional(query, palette, p.ncolors,
        configuration, workspace);
    if (best >= p.ncolors) {
        atomic_store_explicit(invalid, 1u, memory_order_relaxed);
        a[i] = float4(0.f);
        return;
    }
    a[i] = float4(query - phased_color(palette, best), float(best));
}

kernel void phased_iterate(device const uchar *source [[buffer(0)]],
    constant uchar *palette [[buffer(1)]],
    device float4 *a [[buffer(2)]], device float4 *b [[buffer(3)]],
    constant PhasedParams &p [[buffer(4)]],
    constant uchar *phase [[buffer(5)]],
    constant uint *positions [[buffer(6)]],
    constant float *weights [[buffer(7)]],
    device const uchar *transparent [[buffer(8)]],
    constant uchar *configuration [[buffer(12)]],
    device const uchar *workspace [[buffer(13)]],
    device atomic_uint *invalid [[buffer(14)]],
    uint lane [[thread_position_in_grid]])
{
    uint tile, position, i, best;
    int x, y, direction, above;
    float3 incoming, query, previous;
    float4 updated, state;

    tile = lane / 1024;
    position = positions[(p.step % 4)*1024 + lane % 1024];
    x = int((tile % p.tiles_x)*64 + position % 64);
    y = int((tile / p.tiles_x)*64 + position / 64);
    if (x >= int(p.width) || y >= int(p.height)) return;
    i = uint(y)*p.width + uint(x);
    updated = float4(0.f);
    if (!transparent[i]) {
        direction = (y & 1) ? -1 : 1;
        above = ((y-1) & 1) ? -1 : 1;
        incoming = phased_send(a,b,phase,weights,x-direction,y,0,p);
        incoming += phased_send(a,b,phase,weights,x-4*direction,y,1,p);
        incoming += phased_send(a,b,phase,weights,x+above,y-1,2,p);
        incoming += phased_send(a,b,phase,weights,x,y-1,3,p);
        incoming += phased_send(a,b,phase,weights,x-above,y-1,4,p);
        incoming += phased_send(a,b,phase,weights,x,y-4,5,p);
        query = clamp(float3(source[3*i],source[3*i+1],source[3*i+2])
            + incoming, 0.f, 255.f);
        state = phased_version(uint(x),uint(y),p.step,phase) ? b[i] : a[i];
        previous = state.xyz + phased_color(palette,uint(state.w));
        query = previous + (query - previous);
        best = lookup_rgb8_fractional(query, palette, p.ncolors,
            configuration, workspace);
        if (best >= p.ncolors) {
            atomic_store_explicit(invalid, 1u, memory_order_relaxed);
        } else {
            updated = float4(query - phased_color(palette,best),float(best));
        }
    }
    /* Earlier phases read new generations; this phase reads the old one.
     * No thread can read another same-phase thread's incomplete write. */
    if (((p.step / 4) + 1) & 1) b[i] = updated;
    else a[i] = updated;
}

kernel void phased_finish(constant uchar *palette [[buffer(1)]],
    device const float4 *a [[buffer(2)]],
    constant PhasedParams &p [[buffer(4)]],
    device const uchar *transparent [[buffer(8)]],
    device const uchar *retained [[buffer(9)]],
    device const uchar *valid [[buffer(10)]],
    device uchar *result [[buffer(11)]],
    constant uchar *configuration [[buffer(12)]],
    device const uchar *workspace [[buffer(13)]],
    device atomic_uint *invalid [[buffer(14)]],
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
        /* Reconstruct the solved fractional query. Retained history is
         * consulted only now and never seeds a later frame's diffusion. */
        fresh = phased_color(palette,best);
        query = a[i].xyz + fresh;
        best = lookup_rgb8_fractional(query, palette, p.ncolors,
            configuration, workspace);
        if (best >= p.ncolors) {
            atomic_store_explicit(invalid, 1u, memory_order_relaxed);
            return;
        }
        fresh = phased_color(palette,best);
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
