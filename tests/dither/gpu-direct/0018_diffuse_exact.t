#!/bin/sh
# Verify GPU error diffusion through exact lookup policies matches NONE.
set -eux

echo "1..1"
set -v

# Keep capability initialization and real dispatch in one timed process.
status=0

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette-direct gpu-direct/0018_diffuse_exact || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP direct GPU palette apply is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - GPU diffusion through exact lookup policies matches NONE"
    exit 0
}
echo "ok 1 - GPU diffusion through exact lookup policies matches NONE"
exit 0
