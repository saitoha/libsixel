#!/bin/sh
# Explicit full clears delta history and permits PNG snapshots.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0017_delta_full_override || {
    echo "not ok 1 - PNG delta full override"
    exit 0
}
echo "ok 1 - PNG delta full override"
exit 0
