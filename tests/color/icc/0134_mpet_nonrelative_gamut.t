#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0134_mpet_nonrelative_gamut || {
    echo "not ok 1 - non-relative MPE tags preserve relative gamut mapping"
    exit 0
}

echo "ok 1 - non-relative MPE tags preserve relative gamut mapping"
exit 0
