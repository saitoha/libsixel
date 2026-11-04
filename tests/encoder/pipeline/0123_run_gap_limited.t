#!/bin/sh
# A long leading gap preserves limited splitting before paint.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0123_run_gap_limited" || {
    echo "not ok 1 - run gap limited"
    exit 0
}
echo "ok 1 - run gap limited"
exit 0
