#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0129_mpet_legacy_mixed_float || {
    echo "not ok 1 - lower intent D2B2 preserves the RGB_F32 legacy pair"
    exit 0
}

echo "ok 1 - lower intent D2B2 preserves the RGB_F32 legacy pair"
exit 0
