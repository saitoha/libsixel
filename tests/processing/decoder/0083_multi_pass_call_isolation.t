#!/bin/sh
# Verify call isolation.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0083_multi_pass_call_isolation" || {
    echo "not ok 1 - call isolation"
    exit 0
}

echo "ok 1 - call isolation"
exit 0
