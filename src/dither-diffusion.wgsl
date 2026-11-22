// SPDX-License-Identifier: MIT
struct DiffusionParams {
    width: u32, height: u32, pass_id: u32, serpentine: u32,
    deadzone: f32, lower: f32, upper: f32, hard: u32,
}
@group(0) @binding(0) var<storage, read> pixels: array<u32>;
@group(0) @binding(2) var<storage, read_write> plane_a: array<vec4f>;
@group(0) @binding(3) var<storage, read_write> plane_b: array<vec4f>;
@group(0) @binding(4) var<uniform> params: DiffusionParams;
@group(0) @binding(5) var<storage, read> tables: array<u32>;

@group(0) @binding(6) var<storage, read_write> invalid: atomic<u32>;

fn unpack_rgb(word: u32) -> vec3f {
    return vec3f(f32(word & 255u), f32((word >> 8u) & 255u),
                 f32((word >> 16u) & 255u));
}

fn diffuse_color(index: u32) -> vec3f { return unpack_rgb(palette[index]); }

fn phase(x: u32, y: u32) -> u32 {
    let at = ((y + 7u) % 64u) * 64u + (x + 43u) % 64u;
    return (tables[at / 4u] >> (8u * (at % 4u) + 6u)) & 3u;
}

fn direction(y: i32) -> i32 {
    return select(1, -1, params.serpentine != 0u && (y & 1) != 0);
}

fn sender(x: i32, y: i32) -> vec4f {
    if (x < 0 || y < 0 || u32(x) >= params.width || u32(y) >= params.height) {
        return vec4f(0);
    }
    let i = u32(y) * params.width + u32(x);
    let step = params.pass_id - 1u;
    let newer = u32(phase(u32(x), u32(y)) < step % 4u);
    // Only read the selected plane. Other lanes may write the opposite plane.
    if (((step / 4u + newer) & 1u) != 0u) { return plane_b[i]; }
    return plane_a[i];
}

// A negative tag suppresses both lookup and write, zero writes a transparent
// state, and a positive tag contributes to the collective query bounds.
fn diffuse_sample(x: u32, y: u32) -> vec4f {
    if (x >= params.width || y >= params.height) { return vec4f(-1); }
    let pass_id = params.pass_id;
    if (pass_id > 0u && pass_id < 9u && phase(x, y) != (pass_id - 1u) % 4u) {
        return vec4f(-1);
    }
    let i = y * params.width + x;
    let word = pixels[i];
    if ((word >> 24u) != 0u) { return vec4f(0); }
    if (pass_id == 9u) {
        return vec4f(diffuse_query(plane_a[i].xyz + diffuse_color(u32(plane_a[i].w))), 1);
    }
    var value = unpack_rgb(word);
    if (pass_id != 0u) { value = candidate(i32(x), i32(y), value); }
    return vec4f(value, 1);
}

fn diffuse_write(x: u32, y: u32, sample: vec4f, best: u32) {
    if (sample.w < 0.0) { return; }
    if (sample.w > 0.0 && best >= arrayLength(&palette)) {
        atomicStore(&invalid, 1u);
        return;
    }
    let i = y * params.width + x;
    var state = vec4f(0);
    if (params.pass_id == 9u) {
        var packed = 65536u;
        if (sample.w > 0.0) { packed = u32(plane_a[i].w) | (best << 8u); }
        plane_b[i] = vec4f(sample.xyz, f32(packed));
        return;
    }
    if (sample.w > 0.0) { state = vec4f(sample.xyz - diffuse_color(best), f32(best)); }
    if (params.pass_id >= 1u && params.pass_id <= 4u) { plane_b[i] = state; }
    else { plane_a[i] = state; }
}
