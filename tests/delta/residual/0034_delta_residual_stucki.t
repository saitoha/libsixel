#!/bin/sh
# Verify delta residual stucki.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0009_delta_residual_stucki || {
    printf '\n%s\n' "not ok 1 - delta residual stucki"
    exit 0
}
printf '\n%s\n' "ok 1 - delta residual stucki"
exit 0
