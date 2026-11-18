#!/bin/sh
# Verify float frame clone preserves color and straight alpha.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0054_float_geometry_state color clone || {
    echo "not ok 1 - float frame clone preserves color and straight alpha"
    exit 0
}
echo "ok 1 - float frame clone preserves color and straight alpha"
exit 0
