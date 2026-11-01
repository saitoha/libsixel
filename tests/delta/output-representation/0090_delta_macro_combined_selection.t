#!/bin/sh
# Delta rejects a combined automatic and fixed-ID macro selection without changing either selector.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0233_delta_macro_combined_selection || {
    echo "not ok 1 - macro combined selection"
    exit 0
}
echo "ok 1 - macro combined selection"
exit 0
