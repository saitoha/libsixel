#!/bin/sh
# A node wider than one output packet preserves its repeat count.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0121_run_large_node" || {
    echo "not ok 1 - run large node"
    exit 0
}
echo "ok 1 - run large node"
exit 0
