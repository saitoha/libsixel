#!/bin/sh
# Verify explicit adaptive sampling accepts an indexed frame via the public API.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0037_palette_sampling_indexed_frame" || {
    echo "not ok 1 - indexed frame adaptive sampling"
    exit 0
}

echo "ok 1 - indexed frame adaptive sampling"
exit 0
