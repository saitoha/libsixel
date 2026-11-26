#!/bin/sh
# Verify ICC HLS delta uses retained residuals in pipeline bands.
set -eux
echo '1..1'
set -v

status=0
SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/icc_output/0001_icc_output \
    delta-feedback 3 "${TOP_SRCDIR}/tests/data/color-numeric" >/dev/null || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP internal parallel dither unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - ICC HLS delta uses retained residuals in pipeline bands'
    exit 0
}
printf '\n%s\n' 'ok 1 - ICC HLS delta uses retained residuals in pipeline bands'
exit 0
