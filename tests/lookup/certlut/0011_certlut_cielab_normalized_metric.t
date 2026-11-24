#!/bin/sh
# Verify certlut cielab normalized metric.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/extended/0001_extended_lookup metric 0 3 || {
    printf '\n%s\n' "not ok 1 - certlut cielab normalized metric"
    exit 0
}

printf '\n%s\n' "ok 1 - certlut cielab normalized metric"
exit 0
