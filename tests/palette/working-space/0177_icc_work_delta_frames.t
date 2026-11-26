#!/bin/sh
# Linear output with ICC working coordinates preserves wire values and paint/keep/change/invalidate state.
# Policy: docs/encoder/palette-application/working-colorspace/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0124_delta_linear_output_linear_frames \
    "icc:${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" || {
    printf '\n%s\n' "not ok 1 - delta ICC working coordinates"
    exit 0
}
printf '\n%s\n' "ok 1 - delta ICC working coordinates"
exit 0
