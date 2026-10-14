#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify public counter exhaustion.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0098_public_counter_exhaustion" || {
    echo "not ok 1 - public counter exhaustion"
    exit 0
}
echo "ok 1 - public counter exhaustion"
exit 0
