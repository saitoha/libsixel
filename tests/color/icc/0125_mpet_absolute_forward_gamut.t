#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0125_mpet_absolute_forward_gamut || {
    printf '\n%s\n' "not ok 1 - absolute forward MPE preserves relative gamut mapping"
    exit 0
}

printf '\n%s\n' "ok 1 - absolute forward MPE preserves relative gamut mapping"
exit 0
