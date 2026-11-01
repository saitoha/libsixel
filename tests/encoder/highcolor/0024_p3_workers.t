#!/bin/sh
# P3 representatives survive completed passes with a four-thread body budget.
# Policy: docs/encoder/output/high-color/README.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threads disabled"
    exit 0
}

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0024_p3_workers || {
    echo "not ok 1 - high-color p3 workers"
    exit 0
}
echo "ok 1 - high-color p3 workers"
exit 0
