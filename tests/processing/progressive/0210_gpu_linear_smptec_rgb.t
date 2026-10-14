#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify GPU repair wire colors and retained history for linear/smptec/rgb.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0210_gpu_linear_smptec_rgb"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu linear smptec rgb # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu linear smptec rgb"
    exit 0
}
echo "ok 1 - gpu linear smptec rgb"
exit 0
