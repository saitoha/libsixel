// SPDX-License-Identifier: MIT
// Accel: DITHER-SIERRA2 webgpu
// Sender offsets and normalized error shares for sierra2.
const dx = array<i32, 7>(
    1, 2, -2, -1, 0, 1, 2);
const dy = array<i32, 7>(
    0, 0, 1, 1, 1, 1, 1);
const weights = array<f32, 7>(
    4.0 / 16.0, 3.0 / 16.0, 1.0 / 16.0, 2.0 / 16.0, 3.0 / 16.0, 2.0 / 16.0, 1.0 / 16.0);
const tap_count = 7u;
