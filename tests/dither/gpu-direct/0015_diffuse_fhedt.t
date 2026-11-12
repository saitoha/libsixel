#!/bin/sh
# Verify GPU diffusion through an FHEDT lookup.
set -eux

echo "1..1"
set -v

# Keep capability initialization and real dispatch in one timed process.
status=0

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette-direct gpu-direct/0015_diffuse_fhedt || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU FHEDT diffusion is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - GPU diffusion through an FHEDT lookup"
    exit 0
}
echo "ok 1 - GPU diffusion through an FHEDT lookup"
exit 0
