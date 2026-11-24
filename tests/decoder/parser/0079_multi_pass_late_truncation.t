#!/bin/sh
# Verify late truncation.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0069_multi_pass_late_truncation" || {
    printf '\n%s\n' "not ok 1 - late truncation"
    exit 0
}

printf '\n%s\n' "ok 1 - late truncation"
exit 0
