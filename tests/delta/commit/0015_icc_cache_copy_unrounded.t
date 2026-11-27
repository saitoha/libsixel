#!/bin/sh
# Verify ICC cache copy preserves unrounded colors.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/icc_output/0001_icc_output \
    cache-unrounded 0 "${TOP_SRCDIR}/tests/data/color-numeric/register-feedback-mpet.icc" >/dev/null || {
    printf '\n%s\n' 'not ok 1 - ICC cache copy preserves unrounded colors'
    exit 0
}
printf '\n%s\n' 'ok 1 - ICC cache copy preserves unrounded colors'
exit 0
