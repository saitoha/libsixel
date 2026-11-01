#!/bin/sh
# Verify paint time colors.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0057_multi_pass_paint_time_colors" || {
    echo "not ok 1 - paint time colors"
    exit 0
}

echo "ok 1 - paint time colors"
exit 0
