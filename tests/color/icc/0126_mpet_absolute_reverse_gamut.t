#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0126_mpet_absolute_reverse_gamut || {
    echo "not ok 1 - absolute reverse MPE preserves relative gamut mapping"
    exit 0
}

echo "ok 1 - absolute reverse MPE preserves relative gamut mapping"
exit 0
