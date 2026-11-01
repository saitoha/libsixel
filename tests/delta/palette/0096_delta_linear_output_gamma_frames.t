#!/bin/sh
# linear output with gamma working coordinates preserves wire values and paint/keep/change/invalidate state.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0123_delta_linear_output_gamma_frames || {
    echo "not ok 1 - delta linear output gamma frames"
    exit 0
}
echo "ok 1 - delta linear output gamma frames"
exit 0
