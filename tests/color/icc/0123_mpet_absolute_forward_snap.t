#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0123_mpet_absolute_forward_snap || {
    echo "not ok 1 - absolute forward MPE preserves relative palette snap"
    exit 0
}

echo "ok 1 - absolute forward MPE preserves relative palette snap"
exit 0
