#!/bin/sh
# Verify all wire percentages independently of color-space interpretation.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0053_percentage_rgb8" || {
    printf '\n%s\n' "not ok 1 - wire percentages reconstruct exact RGB8"
    exit 0
}

printf '\n%s\n' "ok 1 - wire percentages reconstruct exact RGB8"
exit 0
