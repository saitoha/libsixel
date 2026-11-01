#!/bin/sh
# Verify audit gamma.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0035_audit_gamma" || {
    echo "not ok 1 - audit gamma"
    exit 0
}
echo "ok 1 - audit gamma"
exit 0
