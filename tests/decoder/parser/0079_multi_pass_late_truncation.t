#!/bin/sh
# Verify late truncation.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0069_multi_pass_late_truncation" || {
    echo "not ok 1 - late truncation"
    exit 0
}

echo "ok 1 - late truncation"
exit 0
