#!/bin/sh
# Verify device-less GPU fixed-tap diffusion carry and deferred resolve.
set -eux

echo "1..1"
set -v

# Keep capability initialization and real dispatch in one timed process.
status=0

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette-direct gpu-direct/0014_diffuse_carry || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP device-less GPU diffusion is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - device-less GPU fixed-tap diffusion carry and deferred resolve"
    exit 0
}
echo "ok 1 - device-less GPU fixed-tap diffusion carry and deferred resolve"
exit 0
