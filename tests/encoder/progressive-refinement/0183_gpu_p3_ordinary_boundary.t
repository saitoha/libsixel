#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Keep ordinary Display P3/GPU rejection outside repair source preparation.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0238_gpu_p3_ordinary_boundary" || {
    echo "not ok 1 - ordinary P3 GPU boundary"
    exit 0
}
echo "ok 1 - ordinary P3 GPU boundary"
exit 0
