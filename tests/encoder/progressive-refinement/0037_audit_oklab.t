#!/bin/sh
# Verify audit oklab.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0037_audit_oklab" || {
    printf '\n%s\n' "not ok 1 - audit oklab"
    exit 0
}
printf '\n%s\n' "ok 1 - audit oklab"
exit 0
