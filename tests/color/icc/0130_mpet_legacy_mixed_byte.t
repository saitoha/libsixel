#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0130_mpet_legacy_mixed_byte || {
    echo "not ok 1 - lower intent D2B2 preserves the RGB8 legacy pair"
    exit 0
}

echo "ok 1 - lower intent D2B2 preserves the RGB8 legacy pair"
exit 0
