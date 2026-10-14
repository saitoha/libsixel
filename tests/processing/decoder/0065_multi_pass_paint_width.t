#!/bin/sh
# Verify paint width.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0065_multi_pass_paint_width" || {
    echo "not ok 1 - paint width"
    exit 0
}

echo "ok 1 - paint width"
exit 0
