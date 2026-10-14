#!/bin/sh
# Verify transparent and black.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0058_multi_pass_transparent_and_black" || {
    echo "not ok 1 - transparent and black"
    exit 0
}

echo "ok 1 - transparent and black"
exit 0
