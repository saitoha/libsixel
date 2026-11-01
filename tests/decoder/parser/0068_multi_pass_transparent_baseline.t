#!/bin/sh
# Verify transparent baseline.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0060_multi_pass_transparent_baseline" || {
    echo "not ok 1 - transparent baseline"
    exit 0
}

echo "ok 1 - transparent baseline"
exit 0
