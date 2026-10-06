#!/bin/sh
# Selecting full before monochrome clears delta history and allows monochrome.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0204_delta_mono_full_override || {
    echo "not ok 1 - mono full override"
    exit 0
}
echo "ok 1 - mono full override"
exit 0
