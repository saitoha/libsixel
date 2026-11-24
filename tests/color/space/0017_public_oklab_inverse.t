#!/bin/sh
# Verify public colorspace oklab inverse.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "colorspace/public_float" "4" || {
    printf '\n%s\n' "not ok 1 - public colorspace oklab inverse"
    exit 0
}
printf '\n%s\n' "ok 1 - public colorspace oklab inverse"
exit 0
