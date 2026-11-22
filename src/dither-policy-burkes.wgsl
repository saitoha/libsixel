// SPDX-License-Identifier: MIT
// Sender offsets and normalized error shares for burkes.
const dx = array<i32, 7>(
    1, 2, -2, -1, 0, 1, 2);
const dy = array<i32, 7>(
    0, 0, 1, 1, 1, 1, 1);
const weights = array<f32, 7>(
    8.0 / 32.0, 4.0 / 32.0, 2.0 / 32.0, 4.0 / 32.0, 8.0 / 32.0, 4.0 / 32.0, 2.0 / 32.0);
const tap_count = 7u;
