#!/bin/sh
# Verify partial alpha rejected.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-refresh/0019_partial_alpha_rejected || {
    echo "not ok 1 - partial alpha rejected"
    exit 0
}
echo "ok 1 - partial alpha rejected"
exit 0
