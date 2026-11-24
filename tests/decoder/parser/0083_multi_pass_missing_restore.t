#!/bin/sh
# Verify missing restore.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0073_multi_pass_missing_restore" || {
    printf '\n%s\n' "not ok 1 - missing restore"
    exit 0
}

printf '\n%s\n' "ok 1 - missing restore"
exit 0
