#!/bin/sh
# Verify clear p2.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0067_multi_pass_clear_p2" || {
    echo "not ok 1 - clear p2"
    exit 0
}

echo "ok 1 - clear p2"
exit 0
