#!/bin/sh
# Verify audit unknown domain.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0026_audit_unknown_domain" || {
    echo "not ok 1 - audit unknown domain"
    exit 0
}
echo "ok 1 - audit unknown domain"
exit 0
