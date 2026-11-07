#!/bin/sh
# Verify exact pixels after repeated within-band rewinds and a scale change.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/aspect/0046_rewind_change_rewind" || {
    echo "not ok 1 - rewind change rewind"
    exit 0
}
echo "ok 1 - rewind change rewind"
exit 0
