#!/bin/sh
# Verify paint height.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0066_multi_pass_paint_height" || {
    printf '\n%s\n' "not ok 1 - paint height"
    exit 0
}

printf '\n%s\n' "ok 1 - paint height"
exit 0
