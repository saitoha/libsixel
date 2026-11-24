#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify public counter exhaustion.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0098_public_counter_exhaustion" || {
    printf '\n%s\n' "not ok 1 - public counter exhaustion"
    exit 0
}
printf '\n%s\n' "ok 1 - public counter exhaustion"
exit 0
