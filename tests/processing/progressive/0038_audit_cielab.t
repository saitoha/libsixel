#!/bin/sh
# Verify audit cielab.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0038_audit_cielab" || {
    echo "not ok 1 - audit cielab"
    exit 0
}
echo "ok 1 - audit cielab"
exit 0
