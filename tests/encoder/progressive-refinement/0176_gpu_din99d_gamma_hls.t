#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify GPU repair wire colors and retained history for din99d/gamma/hls.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0231_gpu_din99d_gamma_hls"
result=$?
set -e
test "$result" != 77 || {
    printf '\n%s\n' "ok 1 - gpu din99d gamma hls # SKIP GPU device unavailable"
    exit 0
}
test "$result" = 0 || {
    printf '\n%s\n' "not ok 1 - gpu din99d gamma hls"
    exit 0
}
printf '\n%s\n' "ok 1 - gpu din99d gamma hls"
exit 0
