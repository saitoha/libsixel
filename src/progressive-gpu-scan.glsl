/* SPDX-License-Identifier: MIT */
#version 450
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_ARB_gpu_shader_int64 : require
layout(local_size_x = 256) in;
struct Packed {
    float x, y, z;
};
vec3 unpack3(Packed v)
{
    return vec3(v.x, v.y, v.z);
}
bool isfinite(float v)
{
    return !isnan(v) && !isinf(v);
}
bvec3 isfinite(vec3 v)
{
    return bvec3(isfinite(v.x), isfinite(v.y), isfinite(v.z));
}
struct Params {
    uint64_t first;
    uint count, sparse, width, plane_width, origin_x, origin_y;
    float stop, threshold;
    uint sample_limit, filtered;
    uint64_t sample_population, sample_offset;
    uint sample_total, sample_first, preselected, reserved;
};
struct Summary {
    uint domain_count, valid_count, invalid_count, eligible_count, nonfinite;
    float maximum, sum;
    uint sample_count;
    uint64_t maximum_position;
};
struct Sample {
    uint64_t position, plane_position;
    float x, y, z, error;
    uint64_t population;
};
/* Scan, reduction and stratified sampling share one owned cache.
 * vulkan-stages: repair_scan repair_reduce repair_sample
 */
layout(std430, binding = 0) readonly buffer B0
{
    Packed source[];
};
layout(std430, binding = 1) readonly buffer B1
{
    Packed history[];
};
layout(std430, binding = 2) readonly buffer B2
{
    uint8_t domain[];
};
layout(std430, binding = 3) readonly buffer B3
{
    uint8_t valid[];
};
layout(std430, binding = 4) readonly buffer B4
{
    uint64_t positions[];
};
layout(std430, binding = 5) buffer B5
{
    float errors[];
};
layout(std430, binding = 6) buffer B6
{
    uint flags[];
};
layout(std430, binding = 7) buffer B7
{
    Summary groups[];
};
layout(std430, binding = 8) buffer B8
{
    Summary result[];
};
layout(std430, binding = 9) buffer B9
{
    Sample samples[];
};
layout(std430, binding = 10) readonly buffer B10
{
    uint8_t selected[];
};
layout(std430, binding = 11) readonly buffer B11
{
    Params p;
};
shared float local_errors[256];
shared uint local_flags[256];
uint64_t position_at(uint index)
{
    return p.sparse != 0 ? positions[index] : p.first + index;
}
uint64_t stratum(uint i)
{
    uint64_t n = p.sample_total, total = p.sample_population;
    return i * (total / n) + (uint64_t(i) * (total % n)) / n;
}
uint64_t sample_rank(uint i)
{
    uint64_t begin = stratum(i), end = stratum(i + 1);
    return begin + (end - begin - 1) / 2;
}
void repair_scan()
{
    uint gid = gl_GlobalInvocationID.x;
    uint tid = gl_LocalInvocationIndex;
    uint group = gl_WorkGroupID.x;
    float error = 0;
    uint flag = 0;
    if (gid < p.count) {
        uint64_t position = position_at(gid);
        if (domain[uint(position)] != 0) {
            flag = 1;
            if (valid[uint(position)] != 0) {
                vec3 a = unpack3(source[uint(position)]);
                vec3 b = unpack3(history[uint(position)]);
                vec3 d = a - b;
                error = d.x * d.x + d.y * d.y + d.z * d.z;
                flag |= 2;
                if (!all(isfinite(a)) || !all(isfinite(b))
                    || !isfinite(error)) {
                    flag |= 8;
                    error = 0;
                } else if ((p.filtered == 0 || selected[gid] != 0)
                    && (p.preselected != 0
                        || (error > p.stop && error >= p.threshold))) {
                    flag |= 4;
                }
            }
        }
        errors[gid] = error;
        flags[gid] = flag;
    }
    local_errors[tid] = error;
    local_flags[tid] = flag;
    barrier();
    if (tid == 0) {
        Summary s = Summary(0, 0, 0, 0, 0, 0, 0, 0, 0);
        for (uint i = 0; i < 256; ++i) {
            uint f = local_flags[i];
            float e = local_errors[i];
            s.domain_count += uint((f & 1) != 0);
            s.invalid_count += uint((f & 1) != 0 && (f & 2) == 0);
            s.eligible_count += uint((f & 4) != 0);
            s.nonfinite |= f & 8;
            if ((f & 2) != 0) {
                if (s.valid_count == 0 || e > s.maximum) {
                    s.maximum = e;
                    s.maximum_position = position_at(group * 256 + i);
                }
                ++s.valid_count;
                s.sum += e;
            }
        }
        if (!isfinite(s.sum))
            s.nonfinite = 1;
        groups[group] = s;
    }
}
void repair_reduce()
{
    uint gid = gl_GlobalInvocationID.x;
    if (gid != 0)
        return;
    Summary s = Summary(0, 0, 0, 0, 0, 0, 0, 0, 0);
    for (uint i = 0; i < (p.count + 255) / 256; ++i) {
        Summary g = groups[i];
        if (g.valid_count != 0
            && (s.valid_count == 0 || g.maximum > s.maximum)) {
            s.maximum = g.maximum;
            s.maximum_position = g.maximum_position;
        }
        s.domain_count += g.domain_count;
        s.valid_count += g.valid_count;
        s.invalid_count += g.invalid_count;
        s.eligible_count += g.eligible_count;
        s.sum += g.sum;
        s.nonfinite |= g.nonfinite;
    }
    if (!isfinite(s.sum))
        s.nonfinite = 1;
    s.sample_count = min(p.sample_limit, s.eligible_count);
    if (p.sample_total != 0) {
        s.sample_count = 0;
        while (p.sample_first + s.sample_count < p.sample_total
            && s.sample_count < p.sample_limit) {
            uint64_t rank = sample_rank(p.sample_first + s.sample_count);
            if (rank >= p.sample_offset + s.eligible_count)
                break;
            if (rank < p.sample_offset) {
                s.nonfinite = 1;
                break;
            }
            ++s.sample_count;
        }
    }
    result[0] = s;
}
void repair_sample()
{
    uint gid = gl_GlobalInvocationID.x;
    uint n = result[0].sample_count;
    if (gid >= n || result[0].nonfinite != 0)
        return;
    uint population = result[0].eligible_count;
    uint begin = gid * population / n;
    uint end = (gid + 1) * population / n;
    uint ordinal = begin + (end - begin) / 2;
    uint64_t weight = end - begin;
    if (p.sample_total != 0) {
        uint index = p.sample_first + gid;
        ordinal = uint(sample_rank(index) - p.sample_offset);
        weight = stratum(index + 1) - stratum(index);
    }
    uint group = 0;
    while (ordinal >= groups[group].eligible_count) {
        ordinal -= groups[group].eligible_count;
        ++group;
    }
    for (uint i = group * 256; i < min(p.count, (group + 1) * 256); ++i) {
        if ((flags[i] & 4) == 0)
            continue;
        if (ordinal != 0) {
            --ordinal;
            continue;
        }
        uint64_t position = position_at(i);
        vec3 color = unpack3(source[uint(position)]);
        Sample s;
        s.position = position;
        s.plane_position
            = (uint64_t(p.origin_y) + position / p.width) * p.plane_width
            + p.origin_x + position % p.width;
        s.x = color.x;
        s.y = color.y;
        s.z = color.z;
        s.error = errors[i];
        s.population = weight;
        samples[gid] = s;
        break;
    }
}
