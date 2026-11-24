#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0102_mpet_rgba_scratch_reuse || {
    printf '\n%s\n' "not ok 1 - ordered RGBA8 uses one allocation and preserves color and alpha"
    exit 0
}

printf '\n%s\n' "ok 1 - ordered RGBA8 uses one allocation and preserves color and alpha"
exit 0
