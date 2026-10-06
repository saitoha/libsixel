#!/bin/sh
# Selecting monochrome after delta rejects without changing options or retained history.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0202_delta_delta_then_mono || {
    echo "not ok 1 - delta then mono"
    exit 0
}
echo "ok 1 - delta then mono"
exit 0
