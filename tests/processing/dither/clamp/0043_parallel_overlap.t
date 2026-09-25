#!/bin/sh
# Verify soft-clamp work bands keep independent overlap state.
# Policy: docs/functionality/clamp-policy.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP internal threads unavailable"
    set -v
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/clamp/0043_parallel_overlap || {
    echo "not ok 1 - parallel overlap matches independently applied bands"
    exit 0
}
echo "ok 1 - parallel overlap matches independently applied bands"
exit 0
