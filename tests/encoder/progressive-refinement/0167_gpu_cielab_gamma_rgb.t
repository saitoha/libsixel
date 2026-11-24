#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify GPU repair wire colors and retained history for cielab/gamma/rgb.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0222_gpu_cielab_gamma_rgb"
result=$?
set -e
test "$result" != 77 || {
    printf '\n%s\n' "ok 1 - gpu cielab gamma rgb # SKIP GPU device unavailable"
    exit 0
}
test "$result" = 0 || {
    printf '\n%s\n' "not ok 1 - gpu cielab gamma rgb"
    exit 0
}
printf '\n%s\n' "ok 1 - gpu cielab gamma rgb"
exit 0
