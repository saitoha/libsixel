#!/bin/sh
# Verify dequant conflict.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0079_multi_pass_dequant_conflict" || {
    printf '\n%s\n' "not ok 1 - dequant conflict"
    exit 0
}

printf '\n%s\n' "ok 1 - dequant conflict"
exit 0
