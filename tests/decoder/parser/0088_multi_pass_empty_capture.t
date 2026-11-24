#!/bin/sh
# Verify empty capture.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0078_multi_pass_empty_capture" || {
    printf '\n%s\n' "not ok 1 - empty capture"
    exit 0
}

printf '\n%s\n' "ok 1 - empty capture"
exit 0
