#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify GPU repair wire colors and retained history for linear/smptec/hls.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0211_gpu_linear_smptec_hls"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu linear smptec hls # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu linear smptec hls"
    exit 0
}
echo "ok 1 - gpu linear smptec hls"
exit 0
