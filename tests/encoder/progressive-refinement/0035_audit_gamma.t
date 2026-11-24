#!/bin/sh
# Verify audit gamma.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0035_audit_gamma" || {
    printf '\n%s\n' "not ok 1 - audit gamma"
    exit 0
}
printf '\n%s\n' "ok 1 - audit gamma"
exit 0
