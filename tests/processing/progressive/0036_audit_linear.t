#!/bin/sh
# Verify audit linear.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0036_audit_linear" || {
    echo "not ok 1 - audit linear"
    exit 0
}
echo "ok 1 - audit linear"
exit 0
