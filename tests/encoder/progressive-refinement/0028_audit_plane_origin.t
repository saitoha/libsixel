#!/bin/sh
# Verify audit plane origin.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0028_audit_plane_origin" || {
    echo "not ok 1 - audit plane origin"
    exit 0
}
echo "ok 1 - audit plane origin"
exit 0
