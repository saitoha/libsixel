#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify float frame byte boundary: oklab nan owned atomic.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0178_float_byte_boundary 2 1 2 1>&2 || {
    printf '\n%s\n' "not ok 1 - frame byte oklab nan owned atomic"
    exit 0
}
printf '\n%s\n' "ok 1 - frame byte oklab nan owned atomic"
exit 0
