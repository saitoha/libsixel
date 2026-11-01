#!/bin/sh
# Verify refine exact.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/delta_native/0237_delta_refine_exact || {
    echo "not ok 1 - refine_exact"
    exit 0
}
echo "ok 1 - refine_exact"
exit 0
