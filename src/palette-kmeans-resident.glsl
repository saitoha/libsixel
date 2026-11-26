/* SPDX-License-Identifier: MIT */
/* Resident RGB888 stages reuse the native Lloyd, Heckbert and lookup kernels.
 * Accel: HECKBERT-RESIDENT-REP vulkan
 * Accel: KMEANS-RESIDENT-COVER vulkan
 * Accel: KMEANS-RESIDENT-HISTOGRAM vulkan
 * Accel: KMEANS-RESIDENT-LOOKUP vulkan
 * Accel: KMEANS-RESIDENT-OBJECTIVE vulkan
 * Accel: KMEANS-RESIDENT-PACK vulkan
 * Accel: KMEANS-RESIDENT-PCA vulkan
 * Accel: KMEANS-RESIDENT-SNAP vulkan
 * vulkan-companion: lookup-policy-6bit-gpu.glsl
 * vulkan-stages: resident_histogram resident_count resident_prefix
 * vulkan-stages: resident_compact resident_moments resident_axis resident_tiles
 * vulkan-stages: resident_merge resident_seed resident_score resident_objective
 * vulkan-stages: resident_pack resident_hb_first resident_hb_accumulate
 * vulkan-stages: resident_hb_prefix resident_hb_compact resident_hb_rep
 * vulkan-stages: resident_snap resident_cover_hist resident_cover_gaps
 * vulkan-stages: resident_cover lookup_6bit_build resident_apply
 */
#version 450
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_arithmetic : require
layout(local_size_x_id = 0) in;
layout(std430, binding = 0) readonly buffer Frame { uint8_t rgb[]; };
layout(std430, binding = 1) readonly buffer Geometry {
    uint width, height, stride, samples, colors, min_iterations;
    float threshold;
    uint cover, soft, grow;
    uint snap[256], tones[256], anchors[26];
} g;
layout(std430, binding = 2) buffer Points { float points[]; };
layout(std430, binding = 3) buffer Weights { float weights[]; };
layout(std430, binding = 4) buffer Centers { float centers[]; };
layout(std430, binding = 5) buffer Params {
    uint points, centers, groups, min_iterations, round_centers;
    float threshold;
} p;
layout(std430, binding = 6) buffer HbParams {
    uint count, colors, total;
    float weight0, weight1, weight2;
} hb;
layout(std430, binding = 7) readonly buffer Order { uint order[]; };
struct Box { uint start, size, sum; float priority; };
layout(std430, binding = 8) readonly buffer Boxes { Box boxes[]; };
layout(std430, binding = 9) readonly buffer Solved { uint solved[]; };
layout(std430, binding = 10) buffer Work { uint work[]; };
layout(std430, binding = 11) buffer Result {
    uint final_count, failed;
    uint8_t palette[];
};
layout(std430, binding = 13) writeonly buffer Indices { uint8_t indices[]; };
layout(std430, binding = 14) buffer Dispatch { uint args[9], run, reverse; };
layout(std430, binding = 15) readonly buffer State {
    uint done, iterations, lloyd_failed;
    float delta;
    uint repairs;
};

/* The histogram dies before PCA reuses its storage. Cover has its own
 * region so the host can clear its histogram at the start of recording. */
const uint KM_COUNTS = 32768u * 4u;
const uint AXIS = 512u * 12u;
const uint KEYS_A = AXIS + 4u;
const uint KEYS_B = KEYS_A + 32768u * 2u;
const uint HB_MOMENTS = 64u * 64u * 64u;
const uint HB_SAMPLES = HB_MOMENTS + 32768u * 4u;
const uint HB_TILES = HB_SAMPLES + 32768u;
const uint COVER_BINS = HB_TILES + 256u;
const uint COVER_GAPS = COVER_BINS + 4096u * 4u;
const uint HB_TILE = 128u;
const uint COVER_NEAR = 1728u;

uint gid()
{
    return gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x
        + gl_GlobalInvocationID.x;
}

uvec3 load_rgb(uint i)
{
    uint j = i * 3u;
    return uvec3(rgb[j], rgb[j + 1u], rgb[j + 2u]);
}

uvec3 sample_rgb(uint i)
{
    uint columns = (g.width + g.stride - 1u) / g.stride;
    return load_rgb((i / columns * g.width + i % columns) * g.stride);
}

uvec3 palette_color(uint i)
{
    uint j = 3u * i;
    return uvec3(palette[j], palette[j + 1u], palette[j + 2u]);
}

void store_color(uint i, uvec3 c)
{
    uint j = 3u * i;
    palette[j] = uint8_t(c.x);
    palette[j + 1u] = uint8_t(c.y);
    palette[j + 2u] = uint8_t(c.z);
}

uint key6(uvec3 a)
{
    return (a.x << 12u) | (a.y << 6u) | a.z;
}

uint pack_rgb(uvec3 c)
{
    return (c.x << 16u) | (c.y << 8u) | c.z;
}

uvec3 unpack_rgb(uint c)
{
    return uvec3(c >> 16u, (c >> 8u) & 255u, c & 255u);
}

uvec4 cell(uint base, uint i)
{
    uint j = base + 4u * i;
    return uvec4(work[j], work[j + 1u], work[j + 2u], work[j + 3u]);
}

void accumulate(uint base, uint i, uvec3 c)
{
    uint j = base + 4u * i;
    atomicAdd(work[j], c.x);
    atomicAdd(work[j + 1u], c.y);
    atomicAdd(work[j + 2u], c.z);
    atomicAdd(work[j + 3u], 1u);
}

vec3 point(uint i)
{
    uint j = 3u * i;
    return vec3(points[j], points[j + 1u], points[j + 2u]);
}

void resident_histogram()
{
    uint i = gid();
    if (i >= g.samples)
        return;
    uvec3 c = sample_rgb(i), b = min(c * 32u / 255u, uvec3(31u));
    accumulate(0u, (b.x << 10u) | (b.y << 5u) | b.z, c);
}

void resident_count()
{
    uint i = gid(), n = 0u;
    for (uint j = i * 128u; j < (i + 1u) * 128u; ++j)
        n += uint(cell(0u, j).w != 0u);
    work[KM_COUNTS + i] = n;
}

void resident_prefix()
{
    uint n = 0u;
    for (uint j = 0u; j < 256u; ++j) {
        uint count = work[KM_COUNTS + j];
        work[KM_COUNTS + j] = n;
        n += count;
    }
    p.points = n;
    p.centers = min(n, g.colors);
    p.groups = (n + 511u) / 512u;
    p.min_iterations = g.min_iterations;
    p.round_centers = 1u;
    p.threshold = g.threshold;
    args[0] = (n + 127u) / 128u;
    args[1] = args[2] = args[4] = args[5] = 1u;
    args[3] = p.groups;
    args[6] = (n + 255u) / 256u;
    args[7] = args[8] = 1u;
}

void resident_compact()
{
    uint i = gid(), dest = work[KM_COUNTS + i];
    for (uint j = i * 128u; j < (i + 1u) * 128u; ++j) {
        uvec4 c = cell(0u, j);
        if (c.w == 0u)
            continue;
        vec3 mean = vec3(c.xyz) / float(c.w);
        points[3u * dest] = mean.x;
        points[3u * dest + 1u] = mean.y;
        points[3u * dest + 2u] = mean.z;
        weights[dest++] = c.w;
    }
}

void store_vec(uint i, vec4 v)
{
    for (uint k = 0u; k < 4u; ++k)
        work[i + k] = floatBitsToUint(v[k]);
}

vec4 load_vec(uint i)
{
    return uintBitsToFloat(uvec4(work[i], work[i + 1u],
        work[i + 2u], work[i + 3u]));
}

void resident_moments()
{
    uint i = gid();
    if (i >= (p.points + 63u) / 64u)
        return;
    vec4 sums = vec4(0), diag = vec4(0), cross_terms = vec4(0);
    for (uint j = i * 64u; j < min((i + 1u) * 64u, p.points); ++j) {
        vec3 a = point(j) - 128.0;
        sums += vec4(a, 1) * weights[j];
        diag.xyz += a * a * weights[j];
        cross_terms.xyz += a * a.yzx * weights[j];
    }
    store_vec(12u * i, sums);
    store_vec(12u * i + 4u, diag);
    store_vec(12u * i + 8u, cross_terms);
}

void resident_axis()
{
    vec4 sums = vec4(0);
    vec3 diag = vec3(0), cross_terms = vec3(0);
    for (uint j = 0u; j < (p.points + 63u) / 64u; ++j) {
        sums += load_vec(12u * j);
        diag += load_vec(12u * j + 4u).xyz;
        cross_terms += load_vec(12u * j + 8u).xyz;
    }
    vec3 d = diag - sums.xyz * sums.xyz / sums.w;
    vec3 c = cross_terms - sums.xyz * sums.yzx / sums.w;
    vec3 a = vec3(1.0);
    for (uint j = 0u; j < 16u; ++j) {
        vec3 q = d * a + c * a.yzx + c.zxy * a.zxy;
        if (dot(q, q) == 0.0)
            break;
        a = normalize(q);
    }
    store_vec(AXIS, vec4(a, sums.w));
}

bool less_key(uvec2 a, uvec2 b)
{
    float x = uintBitsToFloat(a.x), y = uintBitsToFloat(b.x);
    return x < y || (x == y && a.y < b.y);
}

uvec2 read_key(uint base, uint i)
{
    return uvec2(work[base + 2u * i], work[base + 2u * i + 1u]);
}

void write_key(uint base, uint i, uvec2 key)
{
    work[base + 2u * i] = key.x;
    work[base + 2u * i + 1u] = key.y;
}

shared uvec2 keys[128];
void resident_tiles()
{
    uint i = gid(), lane = gl_LocalInvocationIndex;
    float projection = i < p.points ?
        dot(point(i), load_vec(AXIS).xyz) : uintBitsToFloat(0x7f800000u);
    keys[lane] = uvec2(floatBitsToUint(projection), i);
    barrier();
    for (uint n = 2u; n <= 128u; n *= 2u)
        for (uint h = n / 2u; h != 0u; h /= 2u) {
            uint other = lane ^ h;
            if (lane < other && less_key(keys[other], keys[lane]) !=
                    ((lane & n) != 0u)) {
                uvec2 key = keys[lane];
                keys[lane] = keys[other];
                keys[other] = key;
            }
            barrier();
        }
    write_key(KEYS_A, i, keys[lane]);
}

void resident_merge()
{
    uint i = gid();
    if (i >= p.points)
        return;
    uint src = reverse == 0u ? KEYS_A : KEYS_B;
    uint dest = reverse == 0u ? KEYS_B : KEYS_A;
    uint base = (i / run) * run, other = base ^ run;
    uint lo = other, hi = min(other + run, p.points);
    uvec2 key = read_key(src, i);
    while (lo < hi) {
        uint mid = (lo + hi) / 2u;
        if (less_key(read_key(src, mid), key))
            lo = mid + 1u;
        else
            hi = mid;
    }
    write_key(dest, min(base, other) + i - base + lo - other, key);
}

void resident_seed()
{
    uint cursor = 0u, src = reverse == 0u ? KEYS_A : KEYS_B;
    float cumulative = 0.0;
    for (uint c = 0u; c < p.centers; ++c) {
        vec4 sum = vec4(0);
        float end = load_vec(AXIS).w * float(c + 1u) / float(p.centers);
        while (cursor < p.points && cumulative < end) {
            uint j = read_key(src, cursor++).y;
            sum += vec4(point(j), 1) * weights[j];
            cumulative += weights[j];
        }
        vec3 mean = sum.w > 0.0 ? sum.xyz / sum.w :
            point(read_key(src, cursor - 1u).y);
        mean = floor(mean + 0.5);
        centers[3u * c] = mean.x;
        centers[3u * c + 1u] = mean.y;
        centers[3u * c + 2u] = mean.z;
    }
}

void resident_score()
{
    uint i = gid();
    if (i >= p.points)
        return;
    float best = uintBitsToFloat(0x7f800000u);
    for (uint c = 0u; c < p.centers; ++c) {
        uint j = 3u * c;
        vec3 d = point(i) - vec3(centers[j], centers[j + 1u], centers[j + 2u]);
        best = min(best, dot(d, d));
    }
    work[i] = floatBitsToUint(best * weights[i]);
}

void resident_objective()
{
    float sum = 0.0;
    for (uint j = 0u; j < p.points; ++j)
        sum += uintBitsToFloat(work[j]);
    work[32768u] = floatBitsToUint(sum);
}

void resident_pack()
{
    uint i = gid();
    if (i == 0u)
        failed = lloyd_failed | uint(iterations == 0u);
    if (i < p.centers)
        store_color(i, uvec3(centers[3u * i],
            centers[3u * i + 1u], centers[3u * i + 2u]));
}

/* Preserve the first grid occurrence of every reversible 6bit bucket. */
uint hb_key(uvec3 c)
{
    return key6(min((c + 4u) >> 2u, uvec3(63u)));
}

uvec3 hb_tone(uvec3 c)
{
    return uvec3(g.tones[c.x], g.tones[c.y], g.tones[c.z]);
}

void resident_hb_first()
{
    uint i = gid();
    if (i >= g.samples)
        return;
    uvec3 c = hb_tone(sample_rgb(i));
    work[HB_SAMPLES + i] = pack_rgb(c);
    atomicMin(work[hb_key(c)], i);
}

void resident_hb_accumulate()
{
    uint i = gid();
    if (i >= g.samples)
        return;
    uvec3 c = unpack_rgb(work[HB_SAMPLES + i]);
    uint first = work[hb_key(c)];
    accumulate(HB_MOMENTS, first, c);
    if (i == first)
        atomicAdd(work[HB_TILES + i / HB_TILE], 1u);
}

void resident_hb_prefix()
{
    uint count = 0u;
    for (uint j = 0u; j < (g.samples + HB_TILE - 1u) / HB_TILE; ++j) {
        uint occupied = work[HB_TILES + j];
        work[HB_TILES + j] = count;
        count += occupied;
    }
    hb.count = count;
    hb.colors = g.colors;
    hb.total = g.samples;
    hb.weight0 = hb.weight1 = hb.weight2 = 1.0;
}

void resident_hb_compact()
{
    uint tile = gl_WorkGroupID.x * gl_NumSubgroups + gl_SubgroupID;
    uint begin = tile * HB_TILE;
    if (begin >= g.samples)
        return;
    uint end = min(g.samples, begin + HB_TILE);
    uint dest = work[HB_TILES + tile];
    /* Intel may use fewer active lanes than the advertised subgroup size. */
    uint width = subgroupAdd(1u);
    uint lane = subgroupExclusiveAdd(1u);
    for (uint base = begin; base < end; base += width) {
        uint i = base + lane;
        uvec4 c = i < end ? cell(HB_MOMENTS, i) : uvec4(0);
        uint occupied = uint(c.w != 0u);
        uint pos = dest + subgroupExclusiveAdd(occupied);
        if (occupied != 0u) {
            uvec3 color = hb_tone((2u * c.xyz + c.w) / (2u * c.w));
            points[4u * pos] = color.x;
            points[4u * pos + 1u] = color.y;
            points[4u * pos + 2u] = color.z;
            points[4u * pos + 3u] = c.w;
        }
        dest += subgroupAdd(occupied);
    }
}

void resident_hb_rep()
{
    uint i = gid(), count = min(hb.count, hb.colors);
    if (i == 0u)
        p.centers = count;
    if (i >= count)
        return;
    if (hb.count <= hb.colors) {
        store_color(i, uvec3(points[4u * i], points[4u * i + 1u],
            points[4u * i + 2u]));
        return;
    }
    Box b = boxes[i];
    if (solved[0] != count || b.size == 0u || b.start >= hb.count ||
            b.size > hb.count - b.start ||
            isnan(b.priority) || isinf(b.priority)) {
        atomicOr(failed, 1u);
        return;
    }
    uint rank = 0u, population = 0u;
    uvec3 sum = uvec3(0u);
    for (uint j = 0u; j < count; ++j) {
        Box other = boxes[j];
        rank += uint(other.priority > b.priority ||
            (other.priority == b.priority && other.start < b.start));
    }
    for (uint j = 0u; j < b.size; ++j) {
        uint index = order[b.start + j];
        if (index >= hb.count) {
            atomicOr(failed, 1u);
            return;
        }
        uint at = 4u * index, mass = uint(points[at + 3u]);
        sum += mass * uvec3(points[at], points[at + 1u], points[at + 2u]);
        population += mass;
    }
    if (population == 0u || population != b.sum) {
        atomicOr(failed, 1u);
        return;
    }
    store_color(rank, (2u * sum + population) / (2u * population));
}

void resident_snap()
{
    uint i = gid();
    if (i < p.centers) {
        uvec3 c = palette_color(i);
        store_color(i, uvec3(g.snap[c.x], g.snap[c.y], g.snap[c.z]));
    }
}

uint sq_rgb(uvec3 a, uvec3 b)
{
    ivec3 d = ivec3(a) - ivec3(b);
    return uint(d.x * d.x + d.y * d.y + d.z * d.z);
}

void resident_cover_hist()
{
    uint i = gid();
    if (i >= g.samples)
        return;
    uvec3 c = sample_rgb(i), b = c >> 4u;
    accumulate(COVER_BINS, (b.x << 8u) | (b.y << 4u) | b.z, c);
}

uvec3 cover_color(uvec4 c)
{
    return c.xyz / c.w;
}

void resident_cover_gaps()
{
    uint i = gid(), gap = 0u;
    uvec4 c = cell(COVER_BINS, i);
    if (c.w >= max(g.samples / 1024u, 2u)) {
        gap = 0xffffffffu;
        for (uint k = 0u; k < p.centers; ++k)
            gap = min(gap, sq_rgb(cover_color(c), palette_color(k)));
    }
    work[COVER_GAPS + i] = gap;
}

uint cover_budget(uint count)
{
    if (g.cover == 4u)
        return count < 32u ? 0u : count < 64u ? 8u : count < 256u ? 14u : 26u;
    return g.cover == 1u ? 8u : g.cover == 2u ? 14u : g.cover == 3u ? 26u : 0u;
}

shared uvec3 colors[256], anchors[26];
shared uvec2 best[256];
shared uint count, found, placed;

uint anchor_gap(uvec3 c, uint n)
{
    uint gap = 0xffffffffu;
    for (uint j = 0u; j < n; ++j)
        gap = min(gap, sq_rgb(c, colors[j]));
    return gap;
}

uvec2 min_pair(uvec2 a, uvec2 b)
{
    return b.x < a.x || (b.x == a.x && b.y < a.y) ? b : a;
}

void resident_cover()
{
    uint lane = gl_LocalInvocationIndex, limit = cover_budget(p.centers);
    if (lane < p.centers)
        colors[lane] = palette_color(lane);
    if (lane == 0u) {
        count = p.centers;
        found = 0u;
    }
    barrier();
    if (g.soft != 0u) {
        for (uint a = 0u; a < limit; ++a) {
            uint rank = 0u;
            for (uint j = lane; j < 4096u; j += 256u)
                rank = max(rank, (work[COVER_GAPS + j] << 12u) | (4095u - j));
            best[lane].x = rank;
            barrier();
            for (uint n = 128u; n != 0u; n /= 2u) {
                if (lane < n)
                    best[lane].x = max(best[lane].x, best[lane + n].x);
                barrier();
            }
            uint winner = best[0].x;
            if ((winner >> 12u) <= COVER_NEAR)
                break;
            uint key = 4095u - (winner & 4095u);
            uvec3 c = cover_color(cell(COVER_BINS, key));
            if (lane == 0u) {
                anchors[a] = c;
                ++found;
            }
            for (uint j = lane; j < 4096u; j += 256u) {
                uint gap = work[COVER_GAPS + j];
                if (gap > COVER_NEAR)
                    work[COVER_GAPS + j] =
                        min(gap, sq_rgb(c, cover_color(cell(COVER_BINS, j))));
            }
            memoryBarrierBuffer();
            barrier();
        }
        if (lane == 0u && g.grow != 0u) {
            uint room = min(found, 256u - count);
            for (uint j = 0u; j < room; ++j)
                colors[count++] = anchors[j];
        }
    } else if (lane == 0u) {
        if (g.grow != 0u) {
            uint original = count;
            for (uint a = 0u; a < limit && count < 256u; ++a) {
                uvec3 c = unpack_rgb(g.anchors[a]);
                if (anchor_gap(c, original) > COVER_NEAR)
                    colors[count++] = c;
            }
            limit = cover_budget(count);
        }
        for (uint a = 0u; a < limit; ++a) {
            uvec3 c = unpack_rgb(g.anchors[a]);
            if (anchor_gap(c, count) > COVER_NEAR)
                anchors[found++] = c;
        }
    }
    barrier();
    for (uint r = 0u; r < 4u; ++r) {
        if (lane == 0u)
            placed = 0u;
        barrier();
        for (uint a = 0u; a < found; ++a) {
            best[lane].x = lane < count ? sq_rgb(anchors[a], colors[lane])
                : 0xffffffffu;
            barrier();
            for (uint n = 128u; n != 0u; n /= 2u) {
                if (lane < n)
                    best[lane].x = min(best[lane].x, best[lane + n].x);
                barrier();
            }
            uint gap = best[0].x;
            barrier();
            if (gap <= COVER_NEAR)
                continue;
            uvec2 rank = uvec2(0xffffffffu);
            if (lane < count) {
                for (uint j = lane + 1u; j < count; ++j)
                    rank = min_pair(rank,
                        uvec2(sq_rgb(colors[lane], colors[j]), lane * 256u + j));
            }
            best[lane] = rank;
            barrier();
            for (uint n = 128u; n != 0u; n /= 2u) {
                if (lane < n)
                    best[lane] = min_pair(best[lane], best[lane + n]);
                barrier();
            }
            if (lane == 0u && best[0].x <= gap / 2u) {
                uint left = best[0].y >> 8u, right = best[0].y & 255u;
                colors[left] = (colors[left] + colors[right]) / 2u;
                colors[right] = anchors[a];
                ++placed;
            }
            barrier();
        }
        if (placed == 0u)
            break;
        barrier();
    }
    if (lane < count)
        store_color(lane, colors[lane]);
    if (lane == 0u)
        p.centers = count;
}

/* The host appends the existing 6bit policy with these bindings. */
#define SIXEL_LOOKUP_WORKSPACE_BINDING 12
#define SIXEL_LOOKUP_BUILDER 1
#define lookup_count p.centers
#define lookup_palette palette

uint lookup_rgb8(uvec3 query, uint ncolors);

void resident_apply()
{
    uint i = gid();
    if (i == 0u)
        final_count = p.centers;
    if (i < g.width * g.height)
        indices[i] = uint8_t(lookup_rgb8(load_rgb(i), p.centers));
}
