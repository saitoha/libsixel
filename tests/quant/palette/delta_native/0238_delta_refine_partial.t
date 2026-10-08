#!/bin/sh
# Verify refine partial.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/delta_native/0238_delta_refine_partial || {
    echo "not ok 1 - refine_partial"
    exit 0
}
echo "ok 1 - refine_partial"
exit 0
