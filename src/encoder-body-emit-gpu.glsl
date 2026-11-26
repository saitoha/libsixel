/* SPDX-License-Identifier: MIT */
/* Accel: BODY-SPELL vulkan */
#version 450
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
/* The serial bucket stage uses one lane per band; the parallel stages use
 * 256 lanes. The host specializes this workgroup size per pipeline. */
layout(local_size_x_id = 0) in;

/* A separate program keeps the span builder's eleven-buffer layout stable.
 * Bindings are shared by all four stages; the host rebinding after growth
 * makes their offsets valid for the entire spelling submission.
 * vulkan-stages: body_emit_lengths body_emit_order body_emit_order_group body_emit_bases body_emit_write
 * vulkan-stages: body_resident_bases
 */
struct EmitParams {
    uint width, words, ncolors, bands;
    uint selectors, limited, nodes, group_limit;
};
layout(std430, binding = 0) readonly buffer B0 { uvec4 spans[]; };
layout(std430, binding = 1) readonly buffer B1 { uint8_t cells[]; };
layout(std430, binding = 2) readonly buffer B2 { uint first_seen[]; };
layout(std430, binding = 3) readonly buffer B3 { uint band_first[]; };
layout(std430, binding = 4) buffer B4 { uint lengths[]; };
layout(std430, binding = 5) readonly buffer B5 { uint registers[]; };
layout(std430, binding = 6) buffer B6 { uint columns[]; };
layout(std430, binding = 7) buffer B7 { uint bits[]; };
layout(std430, binding = 8) buffer B8 { uint sorted[]; };
layout(std430, binding = 9) buffer B9 { uvec4 entries[]; };
layout(std430, binding = 10) buffer B10 { uint band_size[]; };
layout(std430, binding = 11) buffer B11 { uint band_bytes[]; };
layout(std430, binding = 12) buffer B12 { uint8_t body_bytes[]; };
layout(std430, binding = 13) readonly buffer B13 { EmitParams e; };

uint emit_invocation()
{
    uint group = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
    return group * 256u + gl_LocalInvocationIndex;
}

uint emit_digits(uint value)
{
    uint count = 1u;
    while (value >= 10u) {
        value /= 10u;
        ++count;
    }
    return count;
}

uint emit_run_length(uint count)
{
    uint length = 0u;
    if (e.limited != 0u && count > 255u) {
        uint parts = (count - 1u) / 255u;
        length = parts * 5u;
        count -= parts * 255u;
    }
    return length + (count <= 3u ? count : 2u + emit_digits(count));
}

uint emit_number(uint pos, uint value)
{
    uint digits = emit_digits(value);
    for (uint i = digits; i > 0u; --i) {
        body_bytes[pos + i - 1u] = uint8_t(48u + value % 10u);
        value /= 10u;
    }
    return pos + digits;
}

uint emit_run(uint pos, uint symbol, uint count)
{
    if (e.limited != 0u && count > 255u) {
        uint parts = (count - 1u) / 255u;
        count -= parts * 255u;
        for (; parts > 0u; --parts) {
            body_bytes[pos++] = uint8_t(33u);
            body_bytes[pos++] = uint8_t(50u);
            body_bytes[pos++] = uint8_t(53u);
            body_bytes[pos++] = uint8_t(53u);
            body_bytes[pos++] = uint8_t(symbol);
        }
    }
    if (count <= 3u) {
        for (; count > 0u; --count) body_bytes[pos++] = uint8_t(symbol);
        return pos;
    }
    body_bytes[pos++] = uint8_t(33u);
    pos = emit_number(pos, count);
    body_bytes[pos++] = uint8_t(symbol);
    return pos;
}

/* Each span's cell runs can be measured independently. */
void body_emit_lengths()
{
    uint t = emit_invocation();
    if (t >= e.nodes) return;
    uvec4 node = spans[t];
    uint width = node.z - node.y;
    uint total = 0u;
    for (uint i = 0u; i < width;) {
        uint value = uint(cells[node.w + i]);
        uint end = i + 1u;
        while (end < width && uint(cells[node.w + end]) == value) ++end;
        total += emit_run_length(end - i);
        i = end;
    }
    lengths[t] = total;
}

uint emit_next(uint base, uint from)
{
    uint word = from / 32u;
    if (word >= e.words) return e.width;
    uint value = bits[base + word] & (0xffffffffu << (from % 32u));
    while (value == 0u) {
        if (++word >= e.words) return e.width;
        value = bits[base + word];
    }
    return min(word * 32u + uint(findLSB(value)), e.width);
}

/* One invocation per band builds the same start buckets and greedy sweeps
 * as sixel_band_emit. At most six painted colors start at a column of a
 * six-row band, so the insertion loop is bounded by a small bucket. */
void body_emit_order()
{
    uint band = gl_WorkGroupID.x;
    if (gl_LocalInvocationIndex != 0u || band >= e.bands) return;
    uint first = band_first[band];
    uint end = band_first[band + 1u];
    if (end - first <= e.group_limit) return;
    uint base = band * (2u * e.width + 1u);
    uint stop = base + e.width + 1u;
    uint bitmap = band * e.words;
    uint ranks = band * e.ncolors;
    for (uint x = 0u; x <= e.width; ++x) columns[base + x] = 0u;
    for (uint w = 0u; w < e.words; ++w) bits[bitmap + w] = 0u;
    for (uint i = first; i < end; ++i) {
        ++columns[base + spans[i].y + 1u];
    }
    for (uint x = 0u; x < e.width; ++x)
        columns[base + x + 1u] += columns[base + x];
    for (uint x = 0u; x < e.width; ++x)
        columns[stop + x] = columns[base + x];
    for (uint i = first; i < end; ++i) {
        uvec4 node = spans[i];
        uint k = first + columns[stop + node.y]++;
        uint j = k;
        while (j > first + columns[base + node.y]) {
            uvec4 prior = spans[sorted[j - 1u]];
            if (prior.z > node.z ||
                    (prior.z == node.z &&
                     first_seen[ranks + prior.x] >
                     first_seen[ranks + node.x])) break;
            sorted[j] = sorted[j - 1u];
            --j;
        }
        sorted[j] = i;
        bits[bitmap + node.y / 32u] |= 1u << (node.y % 32u);
    }
    uint pos = band > 0u ? 1u : 0u;
    uint entry = first;
    uint x = 0u;
    uint current_color = 0xffffffffu;
    uint sx = emit_next(bitmap, 0u);
    while (sx < e.width) {
        uint dollar = 0u;
        if (x > sx) {
            dollar = 1u;
            x = 0u;
        }
        do {
            uint index = sorted[first + columns[base + sx]];
            uvec4 node = spans[index];
            uint selector = e.selectors != 0u && current_color != node.x
                ? 1u : 0u;
            entries[entry++] = uvec4(index, pos, x,
                dollar | (selector << 1u) | (band << 2u));
            pos += dollar +
                (selector != 0u ? 1u + emit_digits(registers[node.x]) : 0u)
                + (x < sx ? emit_run_length(sx - x) : 0u)
                + lengths[index];
            if (selector != 0u) current_color = node.x;
            dollar = 0u;
            x = node.z;
            if (++columns[base + sx] == columns[stop + sx])
                bits[bitmap + sx / 32u] &= ~(1u << (sx % 32u));
            sx = emit_next(bitmap, x);
        } while (sx < e.width);
        sx = emit_next(bitmap, 0u);
    }
    band_size[band] = pos;
}

/* The Metal serial order stage uses start-column buckets and greedily
 * consumes each bucket by farther end and later first-seen register. Here
 * one workgroup builds those buckets in parallel; the head prefix is also
 * the table that finds the next span start. Span lengths get threadgroup
 * prefix offsets just as in Metal's grouped stage. */
#define BODY_GROUP_NODES 2048u
#define BODY_GROUP_THREADS 256u
#define BODY_GROUP_COLUMNS 2048u
shared uint group_head[BODY_GROUP_COLUMNS + 1u];
shared uint group_stop[BODY_GROUP_COLUMNS];
shared uint group_slots[BODY_GROUP_NODES];
shared uint group_spans[BODY_GROUP_NODES];
shared uint8_t group_colors[BODY_GROUP_NODES];
shared uint group_consumed[BODY_GROUP_NODES / 32u];
shared uint group_partial[BODY_GROUP_THREADS];

void body_emit_order_group()
{
    uint band = gl_WorkGroupID.x;
    uint thread = gl_LocalInvocationIndex;
    if (band >= e.bands) return;
    uint first = band_first[band];
    uint n = band_first[band + 1u] - first;
    if (n > e.group_limit) return;
    uint rank = band * e.ncolors;

    for (uint x = thread; x <= e.width; x += BODY_GROUP_THREADS)
        group_head[x] = 0u;
    for (uint i = thread; i < BODY_GROUP_NODES / 32u;
            i += BODY_GROUP_THREADS) group_consumed[i] = 0u;
    barrier();
    for (uint i = thread; i < n; i += BODY_GROUP_THREADS)
        atomicAdd(group_head[spans[first + i].y + 1u], 1u);
    barrier();
    /* Each lane scans eight adjacent columns. The lane totals then form
     * the cross-lane prefix, leaving head[x] as the first sorted slot
     * whose start is at least x. */
    uint run = 0u;
    for (uint x = thread * 8u;
            x < min((thread + 1u) * 8u, e.width); ++x) {
        run += group_head[x + 1u];
        group_head[x + 1u] = run;
    }
    group_partial[thread] = run;
    barrier();
    if (thread == 0u) {
        uint total = 0u;
        for (uint i = 0u; i < BODY_GROUP_THREADS; ++i) {
            uint count = group_partial[i];
            group_partial[i] = total;
            total += count;
        }
    }
    barrier();
    for (uint x = thread * 8u;
            x < min((thread + 1u) * 8u, e.width); ++x)
        group_head[x + 1u] += group_partial[thread];
    barrier();
    for (uint x = thread; x < e.width; x += BODY_GROUP_THREADS)
        group_stop[x] = group_head[x];
    barrier();
    for (uint i = thread; i < n; i += BODY_GROUP_THREADS) {
        uint sx = spans[first + i].y;
        uint pos = atomicAdd(group_stop[sx], 1u);
        group_slots[pos] = i;
    }
    barrier();
    for (uint x = thread; x < e.width; x += BODY_GROUP_THREADS) {
        uint begin = group_head[x];
        uint end = group_head[x + 1u];
        for (uint i = begin + 1u; i < end; ++i) {
            uint slot = group_slots[i];
            uvec4 node = spans[first + slot];
            uint node_rank = first_seen[rank + node.x];
            uint j = i;
            while (j > begin) {
                uint prior_slot = group_slots[j - 1u];
                uvec4 prior = spans[first + prior_slot];
                uint prior_rank = first_seen[rank + prior.x];
                if (prior.z > node.z ||
                        (prior.z == node.z && prior_rank > node_rank))
                    break;
                group_slots[j] = prior_slot;
                --j;
            }
            group_slots[j] = slot;
        }
    }
    barrier();
    for (uint i = thread; i < n; i += BODY_GROUP_THREADS) {
        uvec4 node = spans[first + group_slots[i]];
        group_spans[i] = (node.y << 16u) | node.z;
        group_colors[i] = uint8_t(node.x);
    }
    barrier();
    if (thread == 0u) {
        uint cursor = 0u;
        uint entry = 0u;
        uint low = 0u;
        uint active_color = 0xffffffffu;
        for (;;) {
            while (low < n && (group_consumed[low / 32u] >>
                    (low % 32u) & 1u) != 0u) ++low;
            if (low >= n) break;
            uint at = low;
            uint dollar = 0u;
            if (cursor > (group_spans[at] >> 16u)) {
                dollar = 1u;
                cursor = 0u;
            }
            for (;;) {
                uint sx = group_spans[at] >> 16u;
                uint mx = group_spans[at] & 0xffffu;
                uint color = uint(group_colors[at]);
                uint select = e.selectors != 0u && active_color != color
                    ? 1u : 0u;
                entries[first + entry++] = uvec4(
                    first + group_slots[at], 0u, cursor,
                    dollar | (select << 1u) | (band << 2u));
                if (select != 0u) active_color = color;
                group_consumed[at / 32u] |= 1u << (at % 32u);
                dollar = 0u;
                cursor = mx;
                uint lo = group_head[cursor];
                uint word = lo / 32u;
                uint available = lo < n ?
                    ~group_consumed[word] &
                    (0xffffffffu << (lo % 32u)) : 0u;
                while (available == 0u && ++word * 32u < n)
                    available = ~group_consumed[word];
                if (available == 0u) break;
                at = word * 32u + uint(findLSB(available));
                if (at >= n) break;
            }
        }
    }
    memoryBarrierBuffer();
    barrier();
    uint per = (n + BODY_GROUP_THREADS - 1u) / BODY_GROUP_THREADS;
    uint lo = thread * per;
    uint hi = min(n, lo + per);
    uint sum = 0u;
    for (uint i = lo; i < hi; ++i) {
        uvec4 item = entries[first + i];
        uvec4 node = spans[item.x];
        uint length = (item.w & 1u) +
            ((item.w & 2u) != 0u ?
                1u + emit_digits(registers[node.x]) : 0u) +
            (item.z < node.y ?
                emit_run_length(node.y - item.z) : 0u) +
            lengths[item.x];
        group_spans[i] = length;
        sum += length;
    }
    group_partial[thread] = sum;
    barrier();
    if (thread == 0u) {
        uint run = band > 0u ? 1u : 0u;
        for (uint i = 0u; i < BODY_GROUP_THREADS; ++i) {
            uint value = group_partial[i];
            group_partial[i] = run;
            run += value;
        }
        band_size[band] = run;
    }
    barrier();
    uint pos = group_partial[thread];
    for (uint i = lo; i < hi; ++i) {
        entries[first + i].y = pos;
        pos += group_spans[i];
    }
}

/* The separator is included in each noninitial band's byte range. */
void body_emit_bases()
{
    if (emit_invocation() != 0u) return;
    uint total = 0u;
    for (uint b = 0u; b < e.bands; ++b) {
        band_bytes[b] = total;
        if (b > 0u) body_bytes[total] = uint8_t(45u);
        total += band_size[b];
    }
    band_bytes[e.bands] = total;
}

/* The offsets above give every span an exclusive output range. */
void body_emit_write()
{
    uint t = emit_invocation();
    if (t >= e.nodes) return;
    uvec4 item = entries[t];
    uvec4 node = spans[item.x];
    uint pos = band_bytes[item.w >> 2u] + item.y;
    if ((item.w & 1u) != 0u)
        body_bytes[pos++] = uint8_t(36u);
    if ((item.w & 2u) != 0u) {
        body_bytes[pos++] = uint8_t(35u);
        pos = emit_number(pos, registers[node.x]);
    }
    if (item.z < node.y)
        pos = emit_run(pos, 63u, node.y - item.z);
    uint width = node.z - node.y;
    for (uint i = 0u; i < width;) {
        uint value = uint(cells[node.w + i]);
        uint end = i + 1u;
        while (end < width && uint(cells[node.w + end]) == value) ++end;
        pos = emit_run(pos, 63u + value, end - i);
        i = end;
    }
}


layout(std430, binding = 14) readonly buffer Palette {
    uint colors, failed;
    uint8_t rgb[];
};
layout(std430, binding = 15) writeonly buffer Indirect { uint groups[]; };

/* Accel: BODY-RESIDENT vulkan */
void body_resident_bases()
{
    bool valid = failed == 0u && colors > 0u && colors <= e.ncolors;
    uint total = 0u, selected_register = 0xffffffffu;
    if (valid) {
        for (uint i = 0u; i < colors; ++i) {
            body_bytes[total++] = uint8_t(35u);
            total = emit_number(total, i);
            body_bytes[total++] = uint8_t(59u);
            body_bytes[total++] = uint8_t(50u);
            for (uint c = 0u; c < 3u; ++c) {
                body_bytes[total++] = uint8_t(59u);
                uint value = (uint(rgb[3u * i + c]) * 100u + 127u) / 255u;
                total = emit_number(total, value);
            }
        }
    }
    for (uint b = 0u; b < e.bands; ++b) {
        uint first = band_first[b], end = band_first[b + 1u], trim = 0u;
        if (first < end) {
            uint reg = spans[entries[first].x].x;
            if (reg == selected_register) {
                // Match the serial writer's register across band separators.
                trim = 1u + emit_digits(reg);
                entries[first].y += trim;
                entries[first].w &= ~2u;
            }
            selected_register = spans[entries[end - 1u].x].x;
        }
        band_bytes[b] = total - trim;
        if (b > 0u)
            body_bytes[total] = uint8_t(45u);
        total += band_size[b] - trim;
    }
    band_bytes[e.bands] = valid ? total : 0u;
    groups[3] = valid ? max(1u, (total + 1023u) / 1024u) : 1u;
    groups[4] = groups[5] = 1u;
}
