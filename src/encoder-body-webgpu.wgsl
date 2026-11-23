// SPDX-License-Identifier: MIT
// The Vulkan body shaders own the wire algorithm: classify columns, join
// spans across small gaps, then emit greedy nonoverlapping sweeps. The
// WebGPU passes keep all span offsets on the device.
struct Params {
    width: u32, height: u32, colors: u32, bands: u32,
    words: u32, pairs: u32, limited: u32, selectors: u32,
    keycolor: i32, has_mask: u32, _pad0: u32, _pad1: u32,
}
@group(0) @binding(0) var<uniform> p: Params;
@group(0) @binding(1) var<storage, read> indexes: array<u32>;
@group(0) @binding(2) var<storage, read_write> occupancy: array<atomic<u32>>;
// Each pair holds its span count and its exclusive offset within the band.
@group(0) @binding(3) var<storage, read_write> pairs: array<vec2u>;
// Each span holds color, start, end, and offset into the cell map.
// The classifier packs four six-row masks into each atomic word.
@group(0) @binding(4) var<storage, read_write> nodes: array<vec4u>;
@group(0) @binding(5) var<storage, read_write> cells: array<atomic<u32>>;
@group(0) @binding(6) var<storage, read_write> seen: array<atomic<u32>>;
@group(0) @binding(7) var<storage, read_write> lengths: array<u32>;
@group(0) @binding(8) var<storage, read_write> bins: array<u32>;
@group(0) @binding(9) var<storage, read_write> sorted: array<u32>;
// Each ordered span holds node index, byte offset in band, x, and flags.
@group(0) @binding(10) var<storage, read_write> entries: array<vec4u>;
// Single arena contains the band's span bounds, lengths and stream offsets.
@group(0) @binding(11) var<storage, read_write> bands: array<vec4u>;
@group(0) @binding(12) var<storage, read_write> body_bytes: array<atomic<u32>>;
@group(0) @binding(13) var<storage, read> mask: array<u32>;
@group(0) @binding(14) var<storage, read> registers: array<u32>;

fn color_at(i: u32) -> u32 {
    return (indexes[i / 4u] >> (8u * (i % 4u))) & 255u;
}

@compute @workgroup_size(256)
fn classify(@builtin(global_invocation_id) id: vec3u) {
    let i = id.x;
    if (i >= p.width * p.height) { return; }
    if (p.has_mask != 0u &&
        ((mask[i / 4u] >> (8u * (i % 4u))) & 255u) != 0u) {
        return;
    }
    let c = color_at(i);
    if (c >= p.colors || i32(c) == p.keycolor) { return; }
    let x = i % p.width;
    let row = i / p.width;
    let band = row / 6u;
    let rank = (row % 6u) * p.width + x;
    let pair = band * p.colors + c;
    let cell = pair * p.width + x;
    atomicOr(&cells[cell / 4u], (1u << (row % 6u)) << (8u * (cell % 4u)));
    atomicOr(&occupancy[pair * p.words + x / 32u], 1u << (x % 32u));
    // Invert the position so a zeroed rank represents an absent color.
    atomicMax(&seen[pair], p.width * 6u - rank);
}

fn next_column(base: u32, start: u32) -> u32 {
    var w = start / 32u;
    if (w >= p.words) { return p.width; }
    var bits = atomicLoad(&occupancy[base + w]) &
        (0xffffffffu << (start % 32u));
    while (bits == 0u) {
        w++;
        if (w >= p.words) { return p.width; }
        bits = atomicLoad(&occupancy[base + w]);
    }
    return min(w * 32u + firstTrailingBit(bits), p.width);
}

fn spans(t: u32, write: bool) {
    if (t >= p.pairs) { return; }
    let base = t * p.words;
    let band = t / p.colors;
    let color = t % p.colors;
    let offsets = pairs[t];
    var n = 0u;
    var sx = next_column(base, 0u);
    while (sx < p.width) {
        var mx = sx + 1u;
        var nx = next_column(base, mx);
        while (nx < p.width && nx - mx <= 9u) {
            mx = nx + 1u;
            nx = next_column(base, mx);
        }
        if (write) {
            nodes[bands[band].x + offsets.y + n] =
                vec4u(color, sx, mx, t * p.width + sx);
        }
        n++;
        sx = nx;
    }
    if (!write) { pairs[t] = vec2u(n, 0u); }
}

@compute @workgroup_size(256)
fn count_spans(@builtin(global_invocation_id) id: vec3u) { spans(id.x, false); }

// Within-band prefixes run independently. A short second prefix assigns
// the frame-wide base for each band.
@compute @workgroup_size(1)
fn span_offsets(@builtin(global_invocation_id) id: vec3u) {
    let band = id.x;
    var n = 0u;
    for (var c = 0u; c < p.colors; c++) {
        let t = band * p.colors + c;
        let count = pairs[t].x;
        pairs[t].y = n;
        n += count;
    }
    bands[band].x = n;
}

@compute @workgroup_size(1)
fn span_bases() {
    var n = 0u;
    for (var band = 0u; band < p.bands; band++) {
        let count = bands[band].x;
        bands[band].x = n;
        n += count;
    }
    bands[p.bands].x = n;
}

@compute @workgroup_size(256)
fn write_spans(@builtin(global_invocation_id) id: vec3u) { spans(id.x, true); }

fn cell_at(i: u32) -> u32 {
    return (atomicLoad(&cells[i / 4u]) >> (8u * (i % 4u))) & 255u;
}

fn digits(number: u32) -> u32 {
    var n = number;
    var d = 1u;
    while (n >= 10u) { n /= 10u; d++; }
    return d;
}

fn run_length(length: u32) -> u32 {
    let parts = select(0u, (length - 1u) / 255u, p.limited != 0u);
    let count = length - parts * 255u;
    return parts * 5u + select(2u + digits(count), count, count <= 3u);
}

@compute @workgroup_size(256)
fn span_lengths(@builtin(global_invocation_id) id: vec3u) {
    let t = id.x;
    if (t >= bands[p.bands].x) { return; }
    let node = nodes[t];
    let width = node.z - node.y;
    var total = 0u;
    var i = 0u;
    while (i < width) {
        let value = cell_at(node.w + i);
        var end = i + 1u;
        while (end < width && cell_at(node.w + end) == value) { end++; }
        total += run_length(end - i);
        i = end;
    }
    lengths[t] = total;
}

fn next_start(base: u32, start: u32) -> u32 {
    var w = start / 32u;
    if (w >= p.words) { return p.width; }
    var bits = bins[base + w] & (0xffffffffu << (start % 32u));
    while (bits == 0u) {
        w++;
        if (w >= p.words) { return p.width; }
        bits = bins[base + w];
    }
    return min(w * 32u + firstTrailingBit(bits), p.width);
}

fn span_wire_length(node: vec4u, index: u32, cursor: u32, flags: u32) -> u32 {
    let selector = select(0u, 1u + digits(registers[node.x]),
        (flags & 2u) != 0u);
    let gap = select(0u, run_length(node.y - cursor), cursor < node.y);
    return (flags & 1u) + selector + gap + lengths[index];
}

const BODY_GROUP_COLUMNS = 2048u;
const BODY_GROUP_NODES = 2048u;
const BODY_GROUP_THREADS = 128u;

fn group_order_supported(count: u32) -> bool {
    return p.width <= BODY_GROUP_COLUMNS && count >= 64u &&
        count <= BODY_GROUP_NODES;
}

@compute @workgroup_size(1)
fn span_order(@builtin(global_invocation_id) id: vec3u) {
    let band = id.x;
    let first = bands[band].x;
    let end = bands[band + 1u].x;
    if (group_order_supported(end - first)) { return; }
    let base = band * (2u * p.width + 1u + p.words);
    let stop = base + p.width + 1u;
    let bitmap = stop + p.width;
    let ranks = band * p.colors;
    for (var x = 0u; x <= p.width; x++) { bins[base + x] = 0u; }
    for (var w = 0u; w < p.words; w++) { bins[bitmap + w] = 0u; }
    for (var i = first; i < end; i++) { bins[base + nodes[i].y + 1u]++; }
    for (var x = 0u; x < p.width; x++) {
        bins[base + x + 1u] += bins[base + x];
        bins[stop + x] = bins[base + x];
    }
    for (var i = first; i < end; i++) {
        let node = nodes[i];
        var j = first + bins[stop + node.y];
        bins[stop + node.y]++;
        while (j > first + bins[base + node.y]) {
            let prior = nodes[sorted[j - 1u]];
            if (prior.z > node.z ||
                (prior.z == node.z && atomicLoad(&seen[ranks + prior.x]) <
                atomicLoad(&seen[ranks + node.x]))) {
                break;
            }
            sorted[j] = sorted[j - 1u];
            j--;
        }
        sorted[j] = i;
        bins[bitmap + node.y / 32u] |= 1u << (node.y % 32u);
    }
    // The first span assumes an unknown active register. byte_offsets
    // removes a redundant selector if the previous band left it active.
    var pos = select(0u, 1u, band != 0u);
    var entry = first;
    var x = 0u;
    var current = 0xffffffffu;
    var sx = next_start(bitmap, 0u);
    while (sx < p.width) {
        var dollar = 0u;
        if (x > sx) { dollar = 1u; x = 0u; }
        loop {
            let index = sorted[first + bins[base + sx]];
            let node = nodes[index];
            let selector = u32(p.selectors != 0u && current != node.x);
            let flags = dollar | (selector << 1u) | (band << 2u);
            entries[entry] = vec4u(index, pos, x, flags);
            entry++;
            pos += span_wire_length(node, index, x, flags);
            current = node.x;
            dollar = 0u;
            x = node.z;
            bins[base + sx]++;
            if (bins[base + sx] == bins[stop + sx]) {
                bins[bitmap + sx / 32u] &= ~(1u << (sx % 32u));
            }
            sx = next_start(bitmap, x);
            if (sx >= p.width) { break; }
        }
        sx = next_start(bitmap, 0u);
    }
    bands[band].y = pos;
    bands[band].z = current;
}

// Like the Vulkan grouped order, bucket by start column, sort each bucket,
// sweep once in lane zero and prefix the final wire lengths in parallel.
// Packed 16-bit column prefixes and reused scratch keep this under 16 KiB.
var<workgroup> group_head: array<atomic<u32>, BODY_GROUP_COLUMNS / 2u>;
var<workgroup> group_work: array<atomic<u32>, BODY_GROUP_NODES>;
var<workgroup> group_consumed: array<u32, BODY_GROUP_NODES / 32u>;
var<workgroup> group_partial: array<u32, BODY_GROUP_THREADS>;
var<workgroup> group_count: u32;

fn group_column(column: u32) -> u32 {
    return (atomicLoad(&group_head[column / 2u]) >>
        (16u * (column % 2u))) & 0xffffu;
}

// Each span occupies at least one painted position in its start column,
// so a six-row band can put at most six spans into one bucket.
fn group_sort_column(first: u32, begin: u32, end: u32, ranks: u32) {
    for (var i = begin + 1u; i < end; i++) {
        let slot = sorted[first + i];
        let node = nodes[first + slot];
        let rank = atomicLoad(&seen[ranks + node.x]);
        var j = i;
        while (j > begin) {
            let previous = sorted[first + j - 1u];
            let prior = nodes[first + previous];
            let prior_rank = atomicLoad(&seen[ranks + prior.x]);
            let precedes = prior_rank < rank ||
                (prior_rank == rank && previous > slot);
            if (prior.z > node.z || (prior.z == node.z && precedes)) {
                break;
            }
            sorted[first + j] = previous;
            j--;
        }
        sorted[first + j] = slot;
    }
    for (var i = begin; i < end; i++) {
        let slot = sorted[first + i];
        let node = nodes[first + slot];
        // 11 bits of slot, 12 of end column, 8 of palette index.
        atomicStore(&group_work[i], slot | (node.z << 11u) | (node.x << 23u));
    }
}

fn group_next(count: u32, start: u32) -> u32 {
    if (start >= count) { return count; }
    var word = start / 32u;
    var available = ~group_consumed[word] & (0xffffffffu << (start % 32u));
    while (available == 0u) {
        word++;
        if (word * 32u >= count) { return count; }
        available = ~group_consumed[word];
    }
    return min(word * 32u + firstTrailingBit(available), count);
}

fn group_sweep(band: u32, first: u32, count: u32) {
    var cursor = 0u;
    var entry = first;
    var current = 0xffffffffu;
    var low = group_next(count, 0u);
    while (low < count) {
        var at = low;
        var dollar = 0u;
        let first_slot = atomicLoad(&group_work[at]) & 2047u;
        if (cursor > nodes[first + first_slot].y) {
            dollar = 1u;
            cursor = 0u;
        }
        while (at < count) {
            let item = atomicLoad(&group_work[at]);
            let color = item >> 23u;
            let selector = u32(p.selectors != 0u && current != color);
            entries[entry] = vec4u(first + (item & 2047u), 0u, cursor,
                dollar | (selector << 1u) | (band << 2u));
            entry++;
            current = color;
            group_consumed[at / 32u] |= 1u << (at % 32u);
            dollar = 0u;
            cursor = (item >> 11u) & 4095u;
            if (cursor >= p.width) { break; }
            at = group_next(count, group_column(cursor));
        }
        low = group_next(count, low);
    }
    bands[band].z = current;
}

fn group_prefix(lane: u32, sum: u32, base: u32) -> u32 {
    group_partial[lane] = sum;
    workgroupBarrier();
    if (lane == 0u) {
        var offset = base;
        for (var i = 0u; i < BODY_GROUP_THREADS; i++) {
            let total = group_partial[i];
            group_partial[i] = offset;
            offset += total;
        }
    }
    workgroupBarrier();
    return group_partial[lane];
}

@compute @workgroup_size(BODY_GROUP_THREADS)
fn span_order_group(@builtin(workgroup_id) id: vec3u,
                    @builtin(local_invocation_index) lane: u32) {
    let band = id.x;
    let first = bands[band].x;
    if (lane == 0u) { group_count = bands[band + 1u].x - first; }
    let count = workgroupUniformLoad(&group_count);
    if (!group_order_supported(count)) { return; }

    // Each lane owns eight consecutive words covering sixteen columns.
    let start = lane * (BODY_GROUP_COLUMNS / (2u * BODY_GROUP_THREADS));
    let limit = min(start + BODY_GROUP_COLUMNS / (2u * BODY_GROUP_THREADS),
        (p.width + 1u) / 2u);
    // WGSL starts workgroup storage at zero. No lane has used these counters.
    for (var i = lane; i < count; i += BODY_GROUP_THREADS) {
        let x = nodes[first + i].y;
        atomicAdd(&group_head[x / 2u], 1u << (16u * (x % 2u)));
    }
    workgroupBarrier();
    var sum = 0u;
    for (var w = start; w < limit; w++) {
        let packed = atomicLoad(&group_head[w]);
        let next = sum + (packed & 0xffffu);
        atomicStore(&group_head[w], sum | (next << 16u));
        sum = next + (packed >> 16u);
    }
    let column_offset = group_prefix(lane, sum, 0u);
    for (var w = start; w < limit; w++) {
        atomicAdd(&group_head[w], column_offset * 0x10001u);
    }
    workgroupBarrier();
    for (var i = lane; i < count; i += BODY_GROUP_THREADS) {
        let x = nodes[first + i].y;
        let slot = group_column(x) + atomicAdd(&group_work[x], 1u);
        sorted[first + slot] = i;
    }
    // All slot cursors are now dead. Reuse them for the ordered spans.
    storageBarrier();
    workgroupBarrier();
    for (var x = lane; x < p.width; x += BODY_GROUP_THREADS) {
        var end = count;
        if (x + 1u < p.width) { end = group_column(x + 1u); }
        group_sort_column(first, group_column(x), end, band * p.colors);
    }
    workgroupBarrier();
    if (lane == 0u) { group_sweep(band, first, count); }
    storageBarrier();
    workgroupBarrier();

    let per_lane = (count + BODY_GROUP_THREADS - 1u) / BODY_GROUP_THREADS;
    let lo = lane * per_lane;
    let hi = min(count, lo + per_lane);
    sum = 0u;
    for (var i = lo; i < hi; i++) {
        let entry = entries[first + i];
        let length =
            span_wire_length(nodes[entry.x], entry.x, entry.z, entry.w);
        atomicStore(&group_work[i], length);
        sum += length;
    }
    // Noninitial bands start with a single '-' before their first span.
    var pos = group_prefix(lane, sum, select(0u, 1u, band != 0u));
    if (lane == BODY_GROUP_THREADS - 1u) {
        bands[band].y = pos + sum;
    }
    for (var i = lo; i < hi; i++) {
        entries[first + i].y = pos;
        pos += atomicLoad(&group_work[i]);
    }
}

fn put_byte(pos: u32, value: u32) {
    // Adjacent spans can own different bytes of the same word.
    atomicOr(&body_bytes[pos / 4u], value << (8u * (pos % 4u)));
}

@compute @workgroup_size(1)
fn byte_offsets() {
    var pos = 0u;
    var last = 0xffffffffu;
    for (var band = 0u; band < p.bands; band++) {
        let first = bands[band].x;
        let end = bands[band + 1u].x;
        var size = bands[band].y;
        var skip = 0u;
        if (end > first) {
            if (p.selectors != 0u && nodes[entries[first].x].x == last) {
                skip = 1u + digits(registers[last]);
            }
            last = bands[band].z;
        }
        bands[band].z = pos;
        bands[band].w = skip;
        if (band != 0u) { put_byte(pos, 45u); }
        pos += size - skip;
    }
    bands[p.bands].z = pos;
}

fn put_number(pos: u32, number: u32) -> u32 {
    let d = digits(number);
    var n = number;
    for (var i = d; i > 0u; i--) {
        put_byte(pos + i - 1u, 48u + n % 10u);
        n /= 10u;
    }
    return pos + d;
}

fn put_run(pos: u32, value: u32, length: u32) -> u32 {
    var at = pos;
    var count = length;
    if (p.limited != 0u) {
        while (count > 255u) {
            put_byte(at, 33u);
            put_byte(at + 1u, 50u);
            put_byte(at + 2u, 53u);
            put_byte(at + 3u, 53u);
            put_byte(at + 4u, value);
            at += 5u;
            count -= 255u;
        }
    }
    if (count <= 3u) {
        for (var i = 0u; i < count; i++) { put_byte(at + i, value); }
        return at + count;
    }
    put_byte(at, 33u);
    at = put_number(at + 1u, count);
    put_byte(at, value);
    return at + 1u;
}

@compute @workgroup_size(256)
fn span_write(@builtin(global_invocation_id) id: vec3u) {
    let t = id.x;
    if (t >= bands[p.bands].x) { return; }
    let item = entries[t];
    let node = nodes[item.x];
    let band = bands[item.w >> 2u];
    let first = band.x == t;
    var pos = band.z + item.y - select(band.w, 0u, first);
    if ((item.w & 1u) != 0u) { put_byte(pos, 36u); pos++; }
    if ((item.w & 2u) != 0u && (!first || band.w == 0u)) {
        put_byte(pos, 35u);
        pos = put_number(pos + 1u, registers[node.x]);
    }
    if (item.z < node.y) { pos = put_run(pos, 63u, node.y - item.z); }
    let width = node.z - node.y;
    var i = 0u;
    while (i < width) {
        let value = cell_at(node.w + i);
        var end = i + 1u;
        while (end < width && cell_at(node.w + end) == value) { end++; }
        pos = put_run(pos, 63u + value, end - i);
        i = end;
    }
}
