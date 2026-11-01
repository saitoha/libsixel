#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify presentation zero limits.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0145_presentation_zero_limits" || {
    echo "not ok 1 - presentation zero limits"
    exit 0
}
echo "ok 1 - presentation zero limits"
exit 0
