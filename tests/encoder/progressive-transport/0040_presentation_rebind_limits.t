#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify presentation rebind limits.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0146_presentation_rebind_limits" || {
    echo "not ok 1 - presentation rebind limits"
    exit 0
}
echo "ok 1 - presentation rebind limits"
exit 0
