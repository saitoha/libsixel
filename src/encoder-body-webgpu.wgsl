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

@compute @workgroup_size(1)
fn span_order(@builtin(global_invocation_id) id: vec3u) {
    let band = id.x;
    let first = bands[band].x;
    let end = bands[band + 1u].x;
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
            entries[entry] = vec4u(index, pos, x,
                dollar | (selector << 1u) | (band << 2u));
            entry++;
            pos += dollar +
                select(0u, 1u + digits(registers[node.x]), selector != 0u)
                + select(0u, run_length(sx - x), x < sx) + lengths[index];
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
