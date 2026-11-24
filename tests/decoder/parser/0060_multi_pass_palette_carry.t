#!/bin/sh
# Verify palette carry.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0056_multi_pass_palette_carry" || {
    printf '\n%s\n' "not ok 1 - palette carry"
    exit 0
}

printf '\n%s\n' "ok 1 - palette carry"
exit 0
