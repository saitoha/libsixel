#!/bin/sh
# Verify missing raster.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0062_multi_pass_missing_raster" || {
    printf '\n%s\n' "not ok 1 - missing raster"
    exit 0
}

printf '\n%s\n' "ok 1 - missing raster"
exit 0
