#!/bin/sh
# Verify anchored overlay.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0055_multi_pass_anchored_overlay" || {
    printf '\n%s\n' "not ok 1 - anchored overlay"
    exit 0
}

printf '\n%s\n' "ok 1 - anchored overlay"
exit 0
