// SPDX-License-Identifier: MIT
// The native count and 64 byte indexes fit in 17 storage words on WebGPU.
// Four consecutive indexes share each word, and zero count means overflow.
struct GridlutRow {
    count: u32,
    indexes: array<u32, 16>,
}

@group(0) @binding(1) var<storage, read> palette: array<u32>;
@group(0) @binding(8) var<storage, read_write> gridlut: array<GridlutRow, 4096>;

fn gridlut_color(index: u32) -> vec3i {
    let word = palette[index];
    return vec3i(i32(word & 255u), i32((word >> 8u) & 255u),
                 i32((word >> 16u) & 255u));
}

// The least farthest-corner distance bounds the winner throughout the
// closed box. Keep each color whose nearest-box distance can meet it.
fn gridlut_build_cell(cell: u32, upper_face: i32) {
    let low = vec3i(i32(cell >> 8u), i32((cell >> 4u) & 15u),
                    i32(cell & 15u)) * 16;
    let high = min(low + upper_face, vec3i(255));
    let count = arrayLength(&palette);
    var bound = 2147483647i;
    for (var i = 0u; i < count; i++) {
        let c = gridlut_color(i);
        let gap = max(c - low, high - c);
        bound = min(bound, dot(gap, gap));
    }
    var kept = 0u;
    var packed = 0u;
    for (var i = 0u; i < count; i++) {
        let c = gridlut_color(i);
        let gap = max(max(low - c, c - high), vec3i(0));
        if (dot(gap, gap) > bound) { continue; }
        // A 65th candidate invalidates the row and selects a full scan.
        if (kept == 64u) {
            gridlut[cell].count = 0u;
            return;
        }
        packed |= i << ((kept % 4u) * 8u);
        kept++;
        if (kept % 4u == 0u) {
            gridlut[cell].indexes[kept / 4u - 1u] = packed;
            packed = 0u;
        }
    }
    if (kept % 4u != 0u) {
        gridlut[cell].indexes[kept / 4u] = packed;
    }
    gridlut[cell].count = kept;
}

@compute @workgroup_size(32)
fn gridlut_build(@builtin(global_invocation_id) id: vec3u) {
    gridlut_build_cell(id.x, 15);
}

@compute @workgroup_size(32)
fn gridlut_build_fractional(@builtin(global_invocation_id) id: vec3u) {
    // Fractional queries include the strip above the last byte in a cell.
    gridlut_build_cell(id.x, 16);
}

fn gridlut_cell(q: vec3u) -> u32 {
    let c = q >> vec3u(4u);
    return (c.x << 8u) | (c.y << 4u) | c.z;
}

fn gridlut_index(cell: u32, count: u32, at: u32) -> u32 {
    if (count == 0u) { return at; }
    let packed = gridlut[cell].indexes[at / 4u];
    return (packed >> ((at % 4u) * 8u)) & 255u;
}

// A strict improvement preserves the first index because every row and
// the overflow scan visit candidates in palette order.
fn lookup(q: vec3i) -> u32 {
    let cell = gridlut_cell(vec3u(q));
    let count = gridlut[cell].count;
    if (count == 1u) { return gridlut_index(cell, count, 0u); }
    let limit = select(count, arrayLength(&palette), count == 0u);
    var best = 0u;
    var minimum = 2147483647i;
    for (var at = 0u; at < limit; at++) {
        let index = gridlut_index(cell, count, at);
        let gap = q - gridlut_color(index);
        let distance = dot(gap, gap);
        if (distance < minimum) {
            best = index;
            minimum = distance;
        }
    }
    return best;
}

fn lookup_fractional(q: vec3f) -> u32 {
    let cell = gridlut_cell(vec3u(clamp(q, vec3f(0), vec3f(255))));
    let count = gridlut[cell].count;
    if (count == 1u) { return gridlut_index(cell, count, 0u); }
    let limit = select(count, arrayLength(&palette), count == 0u);
    var best = 0u;
    var minimum = 1e30f;
    for (var at = 0u; at < limit; at++) {
        let index = gridlut_index(cell, count, at);
        let gap = q - vec3f(gridlut_color(index));
        let distance = dot(gap, gap);
        if (distance < minimum) {
            best = index;
            minimum = distance;
        }
    }
    return best;
}
