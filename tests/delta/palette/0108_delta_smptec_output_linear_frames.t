#!/bin/sh
# smptec output with linear working coordinates preserves wire values and paint/keep/change/invalidate state.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0135_delta_smptec_output_linear_frames || {
    printf '\n%s\n' "not ok 1 - delta smptec output linear frames"
    exit 0
}
printf '\n%s\n' "ok 1 - delta smptec output linear frames"
exit 0
