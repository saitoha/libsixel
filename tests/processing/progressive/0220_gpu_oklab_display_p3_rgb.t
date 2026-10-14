#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify GPU repair wire colors and retained history for oklab/display_p3/rgb.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0220_gpu_oklab_display_p3_rgb"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu oklab display p3 rgb # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu oklab display p3 rgb"
    exit 0
}
echo "ok 1 - gpu oklab display p3 rgb"
exit 0
