#!/bin/sh
# Verify output ICC delta compares the inverse working colors.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/icc_output/0001_icc_output \
    delta-threshold 1 "${TOP_SRCDIR}/tests/data/color-numeric/register-feedback-mpet.icc" >/dev/null || {
    printf '\n%s\n' 'not ok 1 - output ICC delta compares the inverse working colors'
    exit 0
}
printf '\n%s\n' 'ok 1 - output ICC delta compares the inverse working colors'
exit 0
