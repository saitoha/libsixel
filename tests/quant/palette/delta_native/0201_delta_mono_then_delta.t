#!/bin/sh
# Selecting delta after monochrome rejects without changing options or retained history.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0201_delta_mono_then_delta || {
    echo "not ok 1 - mono then delta"
    exit 0
}
echo "ok 1 - mono then delta"
exit 0
