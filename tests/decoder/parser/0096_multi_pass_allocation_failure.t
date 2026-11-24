#!/bin/sh
# Verify allocation failure.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0086_multi_pass_allocation_failure" || {
    printf '\n%s\n' "not ok 1 - allocation failure"
    exit 0
}

printf '\n%s\n' "ok 1 - allocation failure"
exit 0
