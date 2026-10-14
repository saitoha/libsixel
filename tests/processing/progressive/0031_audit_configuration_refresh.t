#!/bin/sh
# Verify audit configuration refresh.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0031_audit_configuration_refresh" || {
    echo "not ok 1 - audit configuration refresh"
    exit 0
}
echo "ok 1 - audit configuration refresh"
exit 0
