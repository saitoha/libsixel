#!/bin/sh
# Verify audit cielab.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0038_audit_cielab" || {
    printf '\n%s\n' "not ok 1 - audit cielab"
    exit 0
}
printf '\n%s\n' "ok 1 - audit cielab"
exit 0
