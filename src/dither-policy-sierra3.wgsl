// SPDX-License-Identifier: MIT
// Sender offsets and normalized error shares for sierra3.
const dx = array<i32, 10>(
    1, 2, -2, -1, 0, 1, 2, -1, 0, 1);
const dy = array<i32, 10>(
    0, 0, 1, 1, 1, 1, 1, 2, 2, 2);
const weights = array<f32, 10>(
    5.0 / 32.0, 3.0 / 32.0, 2.0 / 32.0, 4.0 / 32.0,
    5.0 / 32.0, 4.0 / 32.0, 2.0 / 32.0, 2.0 / 32.0,
    3.0 / 32.0, 2.0 / 32.0);
const tap_count = 10u;
