#!/bin/sh
# Verify prepare full recovery.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0063_prepare_full_recovery" || {
    echo "not ok 1 - prepare full recovery"
    exit 0
}
echo "ok 1 - prepare full recovery"
exit 0
