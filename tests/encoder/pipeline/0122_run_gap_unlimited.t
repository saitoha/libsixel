#!/bin/sh
# A long leading gap preserves its unlimited repeat spelling.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0122_run_gap_unlimited" || {
    echo "not ok 1 - run gap unlimited"
    exit 0
}
echo "ok 1 - run gap unlimited"
exit 0
