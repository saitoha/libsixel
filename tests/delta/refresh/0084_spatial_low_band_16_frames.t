#!/bin/sh
# Verify spatial low band 16 frames.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0084_spatial_low_band_16_frames || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - spatial low band 16 frames"
    exit 0
}
printf '\n%s\n' "ok 1 - spatial low band 16 frames"
exit 0
