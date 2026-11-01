#!/bin/sh
# Verify delta builtin workers.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threads disabled"
    exit 0
}

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0040_delta_builtin_workers || {
    echo "not ok 1 - delta builtin workers"
    exit 0
}
echo "ok 1 - delta builtin workers"
exit 0
