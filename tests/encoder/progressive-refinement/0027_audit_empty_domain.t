#!/bin/sh
# Verify audit empty domain.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0027_audit_empty_domain" || {
    printf '\n%s\n' "not ok 1 - audit empty domain"
    exit 0
}
printf '\n%s\n' "ok 1 - audit empty domain"
exit 0
