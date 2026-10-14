#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify GPU repair wire colors and retained history for cielab/gamma/hls.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0223_gpu_cielab_gamma_hls"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu cielab gamma hls # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu cielab gamma hls"
    exit 0
}
echo "ok 1 - gpu cielab gamma hls"
exit 0
