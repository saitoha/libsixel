#!/bin/sh
# Verify prefix whitespace.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0074_multi_pass_prefix_whitespace" || {
    echo "not ok 1 - prefix whitespace"
    exit 0
}

echo "ok 1 - prefix whitespace"
exit 0
