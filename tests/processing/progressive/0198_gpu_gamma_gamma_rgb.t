#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify GPU repair wire colors and retained history for gamma/gamma/rgb.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0198_gpu_gamma_gamma_rgb"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu gamma gamma rgb # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu gamma gamma rgb"
    exit 0
}
echo "ok 1 - gpu gamma gamma rgb"
exit 0
