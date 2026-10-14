#!/bin/sh
# Verify audit empty domain.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0027_audit_empty_domain" || {
    echo "not ok 1 - audit empty domain"
    exit 0
}
echo "ok 1 - audit empty domain"
exit 0
