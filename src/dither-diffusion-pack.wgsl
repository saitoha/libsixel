// SPDX-License-Identifier: MIT
// Accel: DITHER-ATKINSON webgpu
// Accel: DITHER-BURKES webgpu
// Accel: DITHER-FS webgpu
// Accel: DITHER-JAJUNI webgpu
// Accel: DITHER-LSO2 webgpu
// Accel: DITHER-SIERRA1 webgpu
// Accel: DITHER-SIERRA2 webgpu
// Accel: DITHER-SIERRA3 webgpu
// Accel: DITHER-STUCKI webgpu
// One lane owns four consecutive pixels, so byte packing needs no atomics.
// The iterative plane supplies raw indexes. The solved plane additionally
// carries the reconstructed query and its lookup for a valid retained pixel.
struct ResolveParams {
    width: u32, height: u32, ncolors: u32, keycolor: u32,
    delta: u32, threshold: u32, metric: u32, columns: u32,
    rows: u32, tiled: u32, selected_threshold: u32, variable: u32,
    format: u32, mask: u32,
    selection: array<u32, 128>, levels: array<u32, 1024>,
}
@group(0) @binding(0) var<storage, read> solved: array<u32>;
@group(0) @binding(1) var<storage, read> colors: array<u32>;
@group(0) @binding(2) var<storage, read> prior: array<u32>;
@group(0) @binding(3) var<storage, read> p: ResolveParams;
@group(0) @binding(4) var<storage, read_write> output: array<u32>;

fn rgb(v: u32) -> vec3f {
    return vec3f(vec3u(v, v >> 8u, v >> 16u) & vec3u(255u));
}

fn cell_index(xy: vec2u, grid: vec2u) -> u32 {
    let cell = ((xy + 1u) * grid - 1u) / vec2u(p.width, p.height);
    return cell.y * grid.x + cell.x;
}

fn threshold(i: u32, gap: vec3f) -> f32 {
    if (p.columns == 0u) { return f32(p.threshold); }
    let xy = vec2u(i % p.width, i / p.width);
    var cell = cell_index(xy, vec2u(p.columns, p.rows));
    if (p.tiled != 0u) { cell = (xy.y % 64u) * 64u + xy.x % 64u; }
    var level = p.selected_threshold;
    if (p.variable != 0u) {
        level = (p.levels[cell / 4u] >> (8u * (cell % 4u))) & 255u;
    }
    if (p.variable != 0u && (p.tiled == 2u || p.tiled == 3u)) {
        if (level == 255u) { return f32(p.threshold); }
        let flags = level;
        level &= 63u;
        cell = cell_index(xy, vec2u(64));
        if ((p.selection[cell / 32u] & (1u << (cell % 32u))) != 0u) {
            if ((flags & 128u) != 0u) { level = 8u; }
            if ((flags & 64u) != 0u && (p.tiled == 2u ||
                    abs(dot(gap, vec3f(54, 183, 19))) <= 1024.0)) {
                level = 16u;
            }
        }
    } else if ((p.selection[cell / 32u] & (1u << (cell % 32u))) == 0u) {
        return f32(p.threshold);
    }
    return f32(min(level, p.threshold));
}

fn classify(i: u32, query: vec3f, old: vec3f, fresh: vec3f) -> u32 {
    let gap = query - old;
    let t = threshold(i, gap);
    var within = all(abs(gap) <= vec3f(t));
    if (p.metric == 2u) { within = dot(gap * gap, vec3f(2, 4, 1)) <= 7.0*t*t; }
    if (within) { return select(1u, 3u, any(gap != vec3f(0))); }
    let d = query - fresh;
    return u32(dot(gap, gap) <= dot(d, d));
}

// The worker-portable layout starts with byte planes for the iterative
// and reconstructed indexes. Fixed kernels have exact byte queries;
// LSO2 preserves three binary32 channels. Only masked frames append bits.
fn field(word: u32, k: u32) -> u32 { return (word >> (8u * k)) & 255u; }

fn mask_start(words: u32, count: u32) -> u32 {
    if (p.format == 1u) { return words; }
    if (p.format == 2u) { return 5u * words; }
    return 2u * words + 3u * count;
}

@compute @workgroup_size(64)
fn freeze(@builtin(workgroup_id) group: vec3u, @builtin(num_workgroups) grid: vec3u,
          @builtin(local_invocation_index) lane: u32) {
    let word = (group.y * grid.x + group.x) * 64u + lane;
    let count = p.width * p.height;
    let words = (count + 3u) / 4u;
    if (word >= words) { return; }
    var raw = 0u;
    var corrected = 0u;
    var bytes = vec3u(0);
    for (var k = 0u; k < 4u && 4u * word + k < count; k++) {
        let i = 4u * word + k;
        let tag = u32(bitcast<f32>(solved[4u * i + 3u]));
        raw |= (tag & 255u) << (8u * k);
        corrected |= ((tag >> 8u) & 255u) << (8u * k);
        if (p.format == 2u) {
            for (var c = 0u; c < 3u; c++) {
                bytes[c] |= u32(bitcast<f32>(solved[4u*i+c])) << (8u*k);
            }
        } else if (p.format == 3u) {
            for (var c = 0u; c < 3u; c++) {
                output[2u*words + 3u*i+c] = solved[4u*i+c];
            }
        }
    }
    output[word] = raw;
    if (p.format > 1u) { output[words + word] = corrected; }
    if (p.format == 2u) {
        for (var c = 0u; c < 3u; c++) { output[(2u+c)*words + word] = bytes[c]; }
    }
    if (p.mask != 0u && word % 8u == 0u) {
        var bits = 0u;
        for (var k = 0u; k < 32u && 4u*word+k < count; k++) {
            let tag = u32(bitcast<f32>(solved[4u*(4u*word+k)+3u]));
            bits |= (tag >> 16u) << k;
        }
        output[mask_start(words, count) + word / 8u] = bits;
    }
}

@compute @workgroup_size(64)
fn main(@builtin(workgroup_id) group: vec3u, @builtin(num_workgroups) grid: vec3u,
        @builtin(local_invocation_index) lane: u32) {
    let word = (group.y * grid.x + group.x) * 64u + lane;
    let count = p.width * p.height;
    let words = (count + 3u) / 4u;
    if (word >= words) { return; }
    var indexes = 0u;
    var skips = 0u;
    for (var k = 0u; k < 4u && word * 4u + k < count; k++) {
        let i = word * 4u + k;
        var tag: u32;
        if (p.format == 0u) {
            tag = u32(bitcast<f32>(solved[4u*i+3u]));
        } else {
            tag = field(solved[word], k);
            if (p.mask != 0u) {
                let bits = solved[mask_start(words, count) + i / 32u];
                tag |= ((bits >> (i % 32u)) & 1u) << 16u;
            }
        }
        var code = tag >> 16u;
        var index = tag & 255u;
        if (code == 0u && p.delta != 0u && (prior[i] >> 24u) != 0u) {
            var q: vec3f;
            if (p.format == 0u) {
                index = (tag >> 8u) & 255u;
                for (var c = 0u; c < 3u; c++) { q[c] = bitcast<f32>(solved[4u*i+c]); }
            } else {
                index = field(solved[words + word], k);
                for (var c = 0u; c < 3u; c++) {
                    if (p.format == 2u) { q[c] = f32(field(solved[(2u+c)*words+word], k)); }
                    else { q[c] = bitcast<f32>(solved[2u*words+3u*i+c]); }
                }
            }
            code = classify(i, q, rgb(prior[i]), rgb(colors[index]));
        }
        indexes |= select(index, p.keycolor, code != 0u) << (8u * k);
        skips |= code << (8u * k);
    }
    output[word] = indexes;
    if (arrayLength(&output) > words) { output[words + word] = skips; }
}
