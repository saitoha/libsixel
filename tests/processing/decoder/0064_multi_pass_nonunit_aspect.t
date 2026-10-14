#!/bin/sh
# Verify nonunit aspect.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0064_multi_pass_nonunit_aspect" || {
    echo "not ok 1 - nonunit aspect"
    exit 0
}

echo "ok 1 - nonunit aspect"
exit 0
