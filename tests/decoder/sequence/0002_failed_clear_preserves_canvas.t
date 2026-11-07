#!/bin/sh
# Policy: docs/decoder/multi-pass.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/sequence/0002_failed_clear_preserves_canvas" || {
    echo "not ok 1 - failed clear preserves the published canvas"
    exit 0
}
echo "ok 1 - failed clear preserves the published canvas"
exit 0
