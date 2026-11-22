// SPDX-License-Identifier: MIT
@group(0) @binding(1) var<storage, read> palette: array<u32>;

// The native RGB8 policy scores four packed entries per step. The register
// occupies the low eight bits of the score to resolve equal distances.
fn lookup(query: vec3i) -> u32 {
    var best = 0xffffffffu;
    let last = arrayLength(&palette) - 1u;
    for (var base = 0u; base <= last; base += 4u) {
        let ix = min(base + vec4u(0, 1, 2, 3), vec4u(last));
        let p = vec4u(palette[ix.x], palette[ix.y], palette[ix.z], palette[ix.w]);
        let r = vec4i(p & vec4u(255)) - query.x;
        let g = vec4i((p >> vec4u(8)) & vec4u(255)) - query.y;
        let b = vec4i(p >> vec4u(16)) - query.z;
        let score = (vec4u(r * r + g * g + b * b) << vec4u(8)) | ix;
        best = min(best, min(min(score.x, score.y), min(score.z, score.w)));
    }
    return best & 255u;
}
