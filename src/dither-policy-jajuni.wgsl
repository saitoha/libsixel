// SPDX-License-Identifier: MIT
// Accel: DITHER-JAJUNI webgpu
// Sender offsets and normalized error shares for jajuni.
const dx = array<i32, 12>(
    1, 2, -2, -1, 0, 1, 2, -2, -1, 0, 1, 2);
const dy = array<i32, 12>(
    0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2);
const weights = array<f32, 12>(
    7.0 / 48.0, 5.0 / 48.0, 3.0 / 48.0, 5.0 / 48.0,
    7.0 / 48.0, 5.0 / 48.0, 3.0 / 48.0, 1.0 / 48.0,
    3.0 / 48.0, 5.0 / 48.0, 3.0 / 48.0, 1.0 / 48.0);
const tap_count = 12u;
