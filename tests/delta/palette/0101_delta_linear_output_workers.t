#!/bin/sh
# linear output preserves retained colors through actual worker dispatch.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threads disabled"
    exit 0
}

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0128_delta_linear_output_workers || {
    echo "not ok 1 - delta linear output workers"
    exit 0
}
echo "ok 1 - delta linear output workers"
exit 0
