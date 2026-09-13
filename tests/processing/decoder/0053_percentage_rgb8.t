#!/bin/sh
# Verify all wire percentages independently of color-space interpretation.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0053_percentage_rgb8" || {
    echo "not ok 1 - wire percentages reconstruct exact RGB8"
    exit 0
}

echo "ok 1 - wire percentages reconstruct exact RGB8"
exit 0
