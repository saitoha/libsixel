/* SPDX-License-Identifier: MIT */
#version 450
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
layout(local_size_x = 256) in;
struct Packed {
    float x, y, z;
};
vec3 unpack3(Packed v)
{
    return vec3(v.x, v.y, v.z);
}
Packed pack3(vec3 v)
{
    return Packed(v.x, v.y, v.z);
}
bool isfinite(float v)
{
    return !isnan(v) && !isinf(v);
}
bvec4 isfinite(vec4 v)
{
    return bvec4(isfinite(v.x), isfinite(v.y), isfinite(v.z), isfinite(v.w));
}
struct Params {
    uint points, centers, groups, min_iterations;
    uint round_centers;
    float threshold;
};
struct State {
    uint done, iterations;
    uint failed;
    float delta;
    uint repairs;
};
/* Independent Lloyd stages preserve ascending point/tile reduction order.
 * vulkan-stages: km_assign km_partial km_reduce km_update
 */
layout(std430, binding = 0) readonly buffer B0
{
    Packed points[];
};
layout(std430, binding = 1) readonly buffer B1
{
    float weights[];
};
layout(std430, binding = 2) buffer B2
{
    Packed centers[];
};
layout(std430, binding = 3) buffer B3
{
    uint membership[];
};
layout(std430, binding = 4) buffer B4
{
    float distances[];
};
layout(std430, binding = 5) buffer B5
{
    vec4 partials[];
};
layout(std430, binding = 6) buffer B6
{
    vec4 sums[];
};
layout(std430, binding = 7) buffer B7
{
    State s;
};
layout(std430, binding = 8) buffer B8
{
    uint maxima[];
};
layout(std430, binding = 9) readonly buffer B9
{
    Params p;
};
shared vec4 moments[512];
shared uint heads[1024], links[512];
void km_assign()
{
    uint gid = gl_GlobalInvocationID.x;
    if (s.done != 0 || gid >= p.points)
        return;
    float weight = weights[gid];
    if (weight <= 0) {
        membership[gid] = 0;
        distances[gid] = 0;
        return;
    }
    vec3 point = unpack3(points[gid]);
    uint owner = s.iterations == 0 ? 0 : membership[gid];
    vec3 d = point - unpack3(centers[owner]);
    float best = d.x * d.x + d.y * d.y + d.z * d.z;
    if (best == 0 && any(notEqual(d, vec3(0))))
        atomicExchange(s.failed, 1u);
    for (uint c = 0; c < p.centers; ++c) {
        d = point - unpack3(centers[c]);
        float distance = d.x * d.x + d.y * d.y + d.z * d.z;
        if (distance == 0 && any(notEqual(d, vec3(0))))
            atomicExchange(s.failed, 1u);
        if (distance < best) {
            best = distance;
            owner = c;
        }
    }
    float residual = best * weight;
    if (!isfinite(residual) || (residual == 0 && best > 0))
        atomicExchange(s.failed, 1u);
    membership[gid] = owner;
    distances[gid] = residual;
}
void km_partial()
{
    uint group = gl_WorkGroupID.x;
    uint lane = gl_LocalInvocationIndex;
    uint lanes = 256u;
    uint begin, count, i, c, owner, selected;
    float weight, farthest;
    vec4 sum;
    if (s.done != 0)
        return;
    begin = group * 512u;
    count = min(512u, p.points - begin);
    for (c = lane; c < p.centers; c += lanes)
        heads[c] = 512u;
    for (i = lane; i < count; i += lanes) {
        weight = weights[begin + i];
        moments[i] = vec4(unpack3(points[begin + i]) * weight, weight);
        links[i] = membership[begin + i];
    }
    barrier();
    if (lane == 0) {
        farthest = -1;
        selected = begin;
        // Prepend in reverse order to preserve ascending sample summation.
        for (i = count; i > 0;) {
            --i;
            owner = links[i];
            links[i] = heads[owner];
            heads[owner] = i;
            // Reverse traversal uses >= so equal residuals keep the lowest
            // index.
            if (distances[begin + i] >= farthest) {
                farthest = distances[begin + i];
                selected = begin + i;
            }
        }
        maxima[group] = selected;
    }
    barrier();
    for (c = lane; c < p.centers; c += lanes) {
        sum = vec4(0);
        for (i = heads[c]; i != 512u; i = links[i]) {
            if (moments[i].w > 0)
                sum += moments[i];
        }
        partials[group * p.centers + c] = sum;
    }
}
void km_reduce()
{
    uint gid = gl_GlobalInvocationID.x;
    if (s.done != 0 || gid >= p.centers)
        return;
    vec4 sum = vec4(0);
    for (uint g = 0; g < p.groups; ++g)
        sum += partials[g * p.centers + gid];
    sums[gid] = sum;
}
void km_update()
{
    uint gid = gl_GlobalInvocationID.x;
    if (gid != 0 || s.done != 0)
        return;
    if (s.failed != 0) {
        s.done = 1;
        return;
    }
    for (uint c = 0; c < p.centers; ++c) {
        if (sums[c].w > 0)
            continue;
        ++s.repairs;
        float farthest = -1;
        uint selected = 0;
        for (uint g = 0; g < p.groups; ++g) {
            uint i = maxima[g];
            if (distances[i] > farthest) {
                farthest = distances[i];
                selected = i;
            }
        }
        uint old = membership[selected];
        float weight = weights[selected];
        vec4 moved = vec4(unpack3(points[selected]) * weight, weight);
        if (sums[old].w > 0) {
            vec4 remaining = sums[old] - moved;
            if (p.round_centers != 0)
                remaining.xyz = max(remaining.xyz, vec3(0));
            remaining.w = max(remaining.w, 0.0f);
            sums[old] = remaining;
        }
        sums[c] = moved;
        membership[selected] = c;
        distances[selected] = 0;
        // Only the consumed donor changes residual. Refresh just its tile.
        uint group = selected / 512u, begin = group * 512u;
        uint end = min(begin + 512u, p.points);
        farthest = -1;
        selected = begin;
        for (uint i = begin; i < end; ++i) {
            if (distances[i] > farthest) {
                farthest = distances[i];
                selected = i;
            }
        }
        maxima[group] = selected;
    }
    float delta = 0;
    for (uint c = 0; c < p.centers; ++c) {
        if (!all(isfinite(sums[c]))) {
            atomicExchange(s.failed, 1u);
            s.done = 1;
            return;
        }
        if (sums[c].w <= 0)
            continue;
        vec3 next = sums[c].xyz / sums[c].w;
        if (p.round_centers != 0)
            next = floor(clamp(next, 0.0f, 255.0f) + 0.5f);
        vec3 d = next - unpack3(centers[c]);
        delta += d.x * d.x + d.y * d.y + d.z * d.z;
        centers[c] = pack3(next);
    }
    ++s.iterations;
    s.delta = delta;
    if (!isfinite(delta)) {
        atomicExchange(s.failed, 1u);
        s.done = 1;
    } else if (delta <= p.threshold && s.iterations >= p.min_iterations)
        s.done = 1;
}
