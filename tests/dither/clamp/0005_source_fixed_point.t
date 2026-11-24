#!/bin/sh
# Verify source fixed point using fixed numeric expectations.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0005_source_fixed_point" || {
    printf '\n%s\n' "not ok 1 - source_fixed_point"
    exit 0
}
printf '\n%s\n' "ok 1 - source_fixed_point"
exit 0
