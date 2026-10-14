#!/bin/sh
# Verify dangling prefix.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0075_multi_pass_dangling_prefix" || {
    echo "not ok 1 - dangling prefix"
    exit 0
}

echo "ok 1 - dangling prefix"
exit 0
