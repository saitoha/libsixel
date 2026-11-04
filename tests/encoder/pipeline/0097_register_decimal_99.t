#!/bin/sh
# Verify register decimal 99.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0097_register_decimal_99"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - register decimal 99 # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - register decimal 99"
    exit 0
}
echo "ok 1 - register decimal 99"
exit 0
