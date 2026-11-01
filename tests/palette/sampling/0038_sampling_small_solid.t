#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify small adaptive samples retain solid-region weights at stride one.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0079_sampling_small_solid" || {
    echo "not ok 1 - small adaptive samples retain solid-region weights"
    exit 0
}
echo "ok 1 - small adaptive samples retain solid-region weights"
exit 0
