#!/bin/sh
# Verify source fixed point using fixed numeric expectations.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0005_source_fixed_point" || {
    echo "not ok 1 - source_fixed_point"
    exit 0
}
echo "ok 1 - source_fixed_point"
exit 0
