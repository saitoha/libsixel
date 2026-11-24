#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0133_mpet_nonrelative_snap || {
    printf '\n%s\n' "not ok 1 - non-relative MPE tags preserve relative palette snap"
    exit 0
}

printf '\n%s\n' "ok 1 - non-relative MPE tags preserve relative palette snap"
exit 0
