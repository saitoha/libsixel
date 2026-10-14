#!/bin/sh
# Verify palette carry.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0056_multi_pass_palette_carry" || {
    echo "not ok 1 - palette carry"
    exit 0
}

echo "ok 1 - palette carry"
exit 0
