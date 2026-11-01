#!/bin/sh
# Verify missing restore.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0073_multi_pass_missing_restore" || {
    echo "not ok 1 - missing restore"
    exit 0
}

echo "ok 1 - missing restore"
exit 0
