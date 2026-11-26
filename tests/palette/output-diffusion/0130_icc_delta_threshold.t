#!/bin/sh
# Verify output ICC delta threshold and retained working color.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/icc_output/0001_icc_output \
    delta-threshold 0 "${TOP_SRCDIR}/tests/data/color-numeric/register-feedback-mpet.icc" >/dev/null || {
    printf '\n%s\n' 'not ok 1 - output ICC delta threshold and retained working color'
    exit 0
}
printf '\n%s\n' 'ok 1 - output ICC delta threshold and retained working color'
exit 0
