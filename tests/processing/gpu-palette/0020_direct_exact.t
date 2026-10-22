#!/bin/sh
# GPU direct lookup matches the CPU exhaustive scan, including keeps.
# Policy: docs/encoder/palette-application/lookup-policy/README.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

status=0

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette "gpu-palette/0020_direct_exact" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - GPU direct lookup matches the CPU exhaustive scan"
    exit 0
}

echo "ok 1 - GPU direct lookup matches the CPU exhaustive scan"
exit 0
