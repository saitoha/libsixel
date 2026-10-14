#!/bin/sh
# Verify skip outside.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0081_multi_pass_skip_outside" || {
    echo "not ok 1 - skip outside"
    exit 0
}

echo "ok 1 - skip outside"
exit 0
