#!/bin/sh
# Verify audit unknown domain.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0026_audit_unknown_domain" || {
    printf '\n%s\n' "not ok 1 - audit unknown domain"
    exit 0
}
printf '\n%s\n' "ok 1 - audit unknown domain"
exit 0
