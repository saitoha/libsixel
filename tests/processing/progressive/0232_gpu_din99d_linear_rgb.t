#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify GPU repair wire colors and retained history for din99d/linear/rgb.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0232_gpu_din99d_linear_rgb"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu din99d linear rgb # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu din99d linear rgb"
    exit 0
}
echo "ok 1 - gpu din99d linear rgb"
exit 0
