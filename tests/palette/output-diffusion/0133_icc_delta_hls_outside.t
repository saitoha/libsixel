#!/bin/sh
# Verify output ICC delta compares the emitted HLS candidate.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/icc_output/0001_icc_output \
    delta-feedback 1 "${TOP_SRCDIR}/tests/data/color-numeric" >/dev/null || {
    printf '\n%s\n' 'not ok 1 - output ICC delta compares the emitted HLS candidate'
    exit 0
}
printf '\n%s\n' 'ok 1 - output ICC delta compares the emitted HLS candidate'
exit 0
