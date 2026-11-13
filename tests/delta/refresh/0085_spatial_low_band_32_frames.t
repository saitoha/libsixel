#!/bin/sh
# Verify spatial low band 32 frames.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0085_spatial_low_band_32_frames || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - spatial low band 32 frames"
    exit 0
}
echo "ok 1 - spatial low band 32 frames"
exit 0
