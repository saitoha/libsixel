// SPDX-License-Identifier: MIT
@group(0) @binding(0) var<storage, read> pixels: array<u32>;
@group(0) @binding(2) var<storage, read_write> indexes: array<u32>;
struct Params {
    count: u32, first: u32, width: u32, strength: f32, save_query: u32,
}
@group(0) @binding(3) var<uniform> params: Params;
@group(0) @binding(5) var<storage, read_write> queries: array<u32>;
@group(0) @binding(4) var<storage, read_write> invalid: atomic<u32>;

fn channel(offset: u32) -> i32 {
    return i32((pixels[offset / 4u] >> ((offset % 4u) * 8u)) & 255u);
}

// Ownership is per word so adjacent output bytes never race. Bound the
// final group by the actual pixel count, independent of upload padding.
@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) id: vec3u) {
    let word = id.x;
    if (word >= arrayLength(&indexes)) { return; }
    var packed = 0u;
    for (var byte = 0u; byte < 4u; byte++) {
        let pixel = word * 4u + byte;
        if (pixel >= params.count) { break; }
        let offset = pixel * 3u;
        let source = vec3i(channel(offset), channel(offset + 1u),
                          channel(offset + 2u));
        let absolute = pixel + params.first;
        let query = positional_query(source, absolute % params.width,
                                     absolute / params.width, params.strength);
        // Only query-mode delta needs the corrected RGB on the host.
        if (params.save_query != 0u) {
            queries[pixel] = u32(query.x) | (u32(query.y) << 8u) |
                             (u32(query.z) << 16u);
        }
        let index = lookup(query);
        if (index >= arrayLength(&palette)) {
            atomicStore(&invalid, 1u);
        }
        packed |= index << (byte * 8u);
    }
    indexes[word] = packed;
}
