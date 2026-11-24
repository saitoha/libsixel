#!/bin/sh
# Verify prepare full recovery.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0063_prepare_full_recovery" || {
    printf '\n%s\n' "not ok 1 - prepare full recovery"
    exit 0
}
printf '\n%s\n' "ok 1 - prepare full recovery"
exit 0
