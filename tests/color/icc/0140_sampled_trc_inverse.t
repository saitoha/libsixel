#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0140_sampled_trc_inverse || {
    printf '\n%s\n' "not ok 1 - sampled trc inverse preserves device boundaries"
    exit 0
}

printf '\n%s\n' "ok 1 - sampled trc inverse preserves device boundaries"
exit 0
