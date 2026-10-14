#!/bin/sh
# Verify unmatched restore.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0072_multi_pass_unmatched_restore" || {
    echo "not ok 1 - unmatched restore"
    exit 0
}

echo "ok 1 - unmatched restore"
exit 0
