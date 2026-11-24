#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify float frame resize down auto.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=auto \
    frame/0018_float_resampling down 4 || {
    printf '\n%s\n' "not ok 1 - float resize down_auto"
    exit 0
}
printf '\n%s\n' "ok 1 - float resize down_auto"
exit 0
