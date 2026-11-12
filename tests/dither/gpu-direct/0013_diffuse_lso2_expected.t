#!/bin/sh
# Verify GPU LSO2 diffusion reproduces the frozen indexes.
set -eux

echo "1..1"
set -v

# Keep capability initialization and real dispatch in one timed process.
status=0

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette-direct gpu-direct/0013_diffuse_lso2_expected || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU diffusion is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - GPU LSO2 diffusion reproduces the frozen indexes"
    exit 0
}
echo "ok 1 - GPU LSO2 diffusion reproduces the frozen indexes"
exit 0
