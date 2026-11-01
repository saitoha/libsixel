#!/bin/sh
# Verify partial frame rejection.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0016_partial_frame_rejection || {
    echo "not ok 1 - partial frame rejection"
    exit 0
}
echo "ok 1 - partial frame rejection"
exit 0
