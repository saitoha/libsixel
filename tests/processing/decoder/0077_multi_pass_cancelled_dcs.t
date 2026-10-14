#!/bin/sh
# Verify cancelled dcs.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0077_multi_pass_cancelled_dcs" || {
    echo "not ok 1 - cancelled dcs"
    exit 0
}

echo "ok 1 - cancelled dcs"
exit 0
