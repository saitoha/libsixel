#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0124_mpet_absolute_reverse_snap || {
    printf '\n%s\n' "not ok 1 - absolute reverse MPE preserves relative palette snap"
    exit 0
}

printf '\n%s\n' "ok 1 - absolute reverse MPE preserves relative palette snap"
exit 0
