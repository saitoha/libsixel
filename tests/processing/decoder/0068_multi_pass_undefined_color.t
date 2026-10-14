#!/bin/sh
# Verify undefined color.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0068_multi_pass_undefined_color" || {
    echo "not ok 1 - undefined color"
    exit 0
}

echo "ok 1 - undefined color"
exit 0
