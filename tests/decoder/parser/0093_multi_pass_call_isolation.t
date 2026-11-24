#!/bin/sh
# Verify call isolation.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0083_multi_pass_call_isolation" || {
    printf '\n%s\n' "not ok 1 - call isolation"
    exit 0
}

printf '\n%s\n' "ok 1 - call isolation"
exit 0
