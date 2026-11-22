// SPDX-License-Identifier: MIT
// Same layout as Metal/Vulkan: a packed palette, 4096 rows of 64 packed
// candidates, and one count per cell. Each vector holds four RGB/index words.
struct CertlutTable {
    colors: array<vec4u, 65600>,
    counts: array<u32, 4096>,
}

@group(0) @binding(1) var<storage, read> palette: array<u32>;
@group(0) @binding(8) var<storage, read_write> certlut: CertlutTable;

const certlut_palette_vectors = 64u;
const certlut_row_vectors = 16u;

fn certlut_rgb(word: u32) -> vec3i {
    return vec3i(i32(word & 255u), i32((word >> 8u) & 255u),
                 i32((word >> 16u) & 255u));
}

fn certlut_pack_color(i: u32, count: u32) -> u32 {
    let index = min(i, count - 1u);
    return (palette[index] & 0xffffffu) | (index << 24u);
}

@compute @workgroup_size(32)
fn certlut_pack(@builtin(global_invocation_id) id: vec3u) {
    let count = arrayLength(&palette);
    let i = 4u * id.x;
    // One invocation writes the whole vector, repeating the last register
    // in a short palette tail so overflow queries can use the same scan.
    certlut.colors[id.x] = vec4u(certlut_pack_color(i, count),
        certlut_pack_color(i + 1u, count), certlut_pack_color(i + 2u, count),
        certlut_pack_color(i + 3u, count));
}

fn certlut_nearest(q: vec3i, row: u32, count: u32) -> u32 {
    var best = 0xffffffffu;
    for (var i = 0u; i < (count + 3u) / 4u; i++) {
        let packed = certlut.colors[row + i];
        let mask = vec4u(255u);
        let dr = vec4i(packed & mask) - q.x;
        let dg = vec4i((packed >> vec4u(8u)) & mask) - q.y;
        let db = vec4i((packed >> vec4u(16u)) & mask) - q.z;
        let scores = (vec4u(dr * dr + dg * dg + db * db) << vec4u(8u)) |
                     (packed >> vec4u(24u));
        best = min(best, min(min(scores.x, scores.y), min(scores.z, scores.w)));
    }
    return best & 255u;
}

fn certlut_row(cell: u32) -> u32 {
    return certlut_palette_vectors + cell * certlut_row_vectors;
}

fn certlut_build_cell(cell: u32, upper_face: i32) {
    let low = vec3i(i32(cell >> 8u), i32((cell >> 4u) & 15u),
                    i32(cell & 15u)) * 16;
    let high = min(low + upper_face, vec3i(255));
    let count = arrayLength(&palette);
    let best = certlut_nearest(low + 8, 0u, count);
    let a = certlut_rgb(certlut.colors[best / 4u][best % 4u]);
    let row = certlut_row(cell);
    var kept = 0u;
    var block = vec4u(0u);
    for (var i = 0u; i < count; i++) {
        let packed = certlut.colors[i / 4u][i % 4u];
        let b = certlut_rgb(packed);
        // The affine distance difference reaches its minimum at this
        // corner. At equality only a lower original index can beat a.
        let corner = select(high, low, a >= b);
        let da = corner - a;
        let db = corner - b;
        let margin = dot(db, db) - dot(da, da);
        if (i != best && (margin > 0 || (margin == 0 && i > best))) {
            continue;
        }
        // Overflow requests a full scan, including the remaining colors.
        if (kept == 4u * certlut_row_vectors) {
            certlut.counts[cell] = 0u;
            return;
        }
        block[kept % 4u] = packed;
        kept++;
        if (kept % 4u == 0u) {
            certlut.colors[row + kept / 4u - 1u] = block;
        }
    }
    // A short vector tail repeats its final candidate on every extra lane.
    let tail = kept % 4u;
    if (tail != 0u) {
        let last = block[tail - 1u];
        for (var i = tail; i < 4u; i++) {
            block[i] = last;
        }
        certlut.colors[row + kept / 4u] = block;
    }
    certlut.counts[cell] = kept;
}

@compute @workgroup_size(32)
fn certlut_build(@builtin(global_invocation_id) id: vec3u) {
    certlut_build_cell(id.x, 15);
}

@compute @workgroup_size(32)
fn certlut_build_fractional(@builtin(global_invocation_id) id: vec3u) {
    // Close the upper face at the next integer to cover fractional queries.
    certlut_build_cell(id.x, 16);
}

fn certlut_cell(q: vec3u) -> u32 {
    let c = q >> vec3u(4u);
    return (c.x << 8u) | (c.y << 4u) | c.z;
}

fn lookup(q: vec3i) -> u32 {
    let cell = certlut_cell(vec3u(q));
    let count = certlut.counts[cell];
    if (count == 0u) { return certlut_nearest(q, 0u, arrayLength(&palette)); }
    let row = certlut_row(cell);
    if (count == 1u) { return certlut.colors[row].x >> 24u; }
    return certlut_nearest(q, row, count);
}

fn lookup_fractional(q: vec3f) -> u32 {
    let cell = certlut_cell(vec3u(clamp(q, vec3f(0), vec3f(255))));
    let kept = certlut.counts[cell];
    let row = select(certlut_row(cell), 0u, kept == 0u);
    let count = select(kept, arrayLength(&palette), kept == 0u);
    if (count == 1u) { return certlut.colors[row].x >> 24u; }
    var best = 0u;
    var minimum = 1e30f;
    // Diffusion retains the unrounded query in all distance comparisons.
    for (var i = 0u; i < count; i++) {
        let packed = certlut.colors[row + i / 4u][i % 4u];
        let index = packed >> 24u;
        let gap = q - vec3f(certlut_rgb(packed));
        let distance = dot(gap, gap);
        if (distance < minimum || (distance == minimum && index < best)) {
            best = index;
            minimum = distance;
        }
    }
    return best;
}
