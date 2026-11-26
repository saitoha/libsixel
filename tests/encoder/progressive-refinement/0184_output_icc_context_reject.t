#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify progressive context conflicts with an ICC output.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/icc_output/0001_icc_output \
    progressive-reject 0 "icc:P${TOP_SRCDIR}/tests/data/color-numeric/register-feedback-lut.icc" >/dev/null || {
    printf '\n%s\n' 'not ok 1 - progressive context conflicts with an ICC output'
    exit 0
}
printf '\n%s\n' 'ok 1 - progressive context conflicts with an ICC output'
exit 0
