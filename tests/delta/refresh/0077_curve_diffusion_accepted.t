#!/bin/sh
# Verify curve diffusion accepted.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0077_curve_diffusion_accepted || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - curve diffusion accepted"
    exit 0
}
echo "ok 1 - curve diffusion accepted"
exit 0
