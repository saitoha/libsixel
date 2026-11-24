#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0129_mpet_legacy_mixed_float || {
    printf '\n%s\n' "not ok 1 - lower intent D2B2 preserves the RGB_F32 legacy pair"
    exit 0
}

printf '\n%s\n' "ok 1 - lower intent D2B2 preserves the RGB_F32 legacy pair"
exit 0
