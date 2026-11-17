#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0113_mpet_gray_scratch || {
    echo "not ok 1 - ordered Gray8 to RGB8 uses no heap scratch and preserves color"
    exit 0
}

echo "ok 1 - ordered Gray8 to RGB8 uses no heap scratch and preserves color"
exit 0
