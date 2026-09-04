#!/bin/sh
# Verify the palette policy resolution lifecycle.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0008_palette_policy_resolution" || {
    echo "not ok 1 - palette policy resolution lifecycle"
    exit 0
}

echo "ok 1 - palette policy resolution lifecycle"
exit 0
