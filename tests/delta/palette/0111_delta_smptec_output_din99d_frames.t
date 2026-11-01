#!/bin/sh
# smptec output with din99d working coordinates preserves wire values and paint/keep/change/invalidate state.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0138_delta_smptec_output_din99d_frames || {
    echo "not ok 1 - delta smptec output din99d frames"
    exit 0
}
echo "ok 1 - delta smptec output din99d frames"
exit 0
