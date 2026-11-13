#!/bin/sh
# Verify GPU delta refinement.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

# Initialize the required lookup and run refinement in the same process.
status=0
SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --require-gpu-lookup none palette/delta_native/0236_delta_refine_gpu || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU direct lookup is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - refine_gpu"
    exit 0
}
echo "ok 1 - refine_gpu"
exit 0
