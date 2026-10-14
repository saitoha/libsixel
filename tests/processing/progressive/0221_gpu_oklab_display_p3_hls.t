#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify GPU repair wire colors and retained history for oklab/display_p3/hls.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0221_gpu_oklab_display_p3_hls"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu oklab display p3 hls # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu oklab display p3 hls"
    exit 0
}
echo "ok 1 - gpu oklab display p3 hls"
exit 0
