// SPDX-License-Identifier: MIT
struct SolidParams {
    width: i32, height: i32, clip_x: i32, clip_y: i32,
    points: u32, rows: u32, tolerance: i32, stride: u32,
}
@group(0) @binding(0) var<storage, read> source: array<u32>;
@group(0) @binding(1) var<storage, read_write> cells: array<atomic<u32>>;
@group(0) @binding(2) var<uniform> p: SolidParams;

fn sample_rgb(x: i32, y: i32) -> vec3i {
    let at = 3u * (u32(y) * u32(p.width) + u32(x));
    let index = at / 4u;
    let shift = 8u * (at % 4u);
    var word = source[index] >> shift;
    if (shift >= 16u) { word |= source[index + 1u] << (32u - shift); }
    return vec3i(i32(word & 255u), i32((word >> 8u) & 255u),
                 i32((word >> 16u) & 255u));
}

// The native scanner tests a five-pixel line on either axis. Checking the
// four neighbors against the tolerance gives the same yes/no result.
fn sample_line(x: i32, y: i32, dx: i32, dy: i32, rgb: vec3i) -> bool {
    if (x - 2 * dx < 0 || x + 2 * dx >= p.width ||
        y - 2 * dy < 0 || y + 2 * dy >= p.height) { return false; }
    let limit = vec3i(p.tolerance);
    return all(abs(sample_rgb(x - dx, y - dy) - rgb) <= limit)
        && all(abs(sample_rgb(x + dx, y + dy) - rgb) <= limit)
        && all(abs(sample_rgb(x - 2 * dx, y - 2 * dy) - rgb) <= limit)
        && all(abs(sample_rgb(x + 2 * dx, y + 2 * dy) - rgb) <= limit);
}

fn sample_point(id: vec2u) -> vec4u {
    if (id.x >= p.points || id.y >= p.rows) { return vec4u(0u); }
    let x = p.clip_x + i32(p.stride * id.x);
    let y = p.clip_y + i32(p.stride * id.y);
    let rgb = sample_rgb(x, y);
    if (!sample_line(x, y, 1, 0, rgb) && !sample_line(x, y, 0, 1, rgb)) {
        return vec4u(0u);
    }
    let high = vec3u(rgb) >> vec3u(4u);
    let key = (high.x << 8u) | (high.y << 4u) | high.z;
    return vec4u(vec3u(rgb), key + 1u);
}

fn add_cell(key: u32, count: u32, rgb: vec3u) {
    let base = (key - 1u) * 5u;
    atomicAdd(&cells[base], count);
    atomicAdd(&cells[base + 1u], rgb.x);
    atomicAdd(&cells[base + 2u], rgb.y);
    atomicAdd(&cells[base + 3u], rgb.z);
}

struct TileCell {
    key: atomic<u32>, count: atomic<u32>,
    r: atomic<u32>, g: atomic<u32>, b: atomic<u32>,
}

// Like the SIMD-group reduction on Metal, coalesce equal keys before
// touching the frame-wide histogram. At most 128 lanes insert into 256
// slots, so linear probing always has spare slots.
var<workgroup> tile: array<TileCell, 256>;

@compute @workgroup_size(32, 4)
fn sample_solid(@builtin(global_invocation_id) id: vec3u,
                @builtin(local_invocation_index) lane: u32) {
    let point = sample_point(id.xy);
    if (point.w != 0u) {
        var slot = ((point.w ^ (point.w >> 8u)) * 251u) & 255u;
        loop {
            let claim = atomicCompareExchangeWeak(&tile[slot].key, 0u, point.w);
            if (claim.exchanged || claim.old_value == point.w) { break; }
            if (claim.old_value != 0u) { slot = (slot + 1u) & 255u; }
        }
        atomicAdd(&tile[slot].count, 1u);
        atomicAdd(&tile[slot].r, point.x);
        atomicAdd(&tile[slot].g, point.y);
        atomicAdd(&tile[slot].b, point.z);
    }
    workgroupBarrier();
    for (var slot = lane; slot < 256u; slot += 128u) {
        let key = atomicLoad(&tile[slot].key);
        if (key != 0u) {
            add_cell(key, atomicLoad(&tile[slot].count),
                vec3u(atomicLoad(&tile[slot].r), atomicLoad(&tile[slot].g),
                      atomicLoad(&tile[slot].b)));
        }
    }
}
