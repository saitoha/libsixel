#!/bin/sh
# Verify raw overlay.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0054_multi_pass_raw_overlay" || {
    printf '\n%s\n' "not ok 1 - raw overlay"
    exit 0
}

printf '\n%s\n' "ok 1 - raw overlay"
exit 0
