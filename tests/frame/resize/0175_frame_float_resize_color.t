#!/bin/sh
# Verify nearest float frame resize preserves color and mask.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0054_float_geometry_state color resize || {
    printf '\n%s\n' "not ok 1 - nearest float frame resize preserves color and mask"
    exit 0
}
printf '\n%s\n' "ok 1 - nearest float frame resize preserves color and mask"
exit 0
