#!/bin/sh
# Verify hls color.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0082_multi_pass_hls_color" || {
    echo "not ok 1 - hls color"
    exit 0
}

echo "ok 1 - hls color"
exit 0
