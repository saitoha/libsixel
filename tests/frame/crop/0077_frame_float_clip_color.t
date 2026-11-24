#!/bin/sh
# Verify float frame crop preserves color and straight alpha.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0054_float_geometry_state color clip || {
    printf '\n%s\n' "not ok 1 - float frame crop preserves color and straight alpha"
    exit 0
}
printf '\n%s\n' "ok 1 - float frame crop preserves color and straight alpha"
exit 0
