#!/bin/sh
# Verify bluenoise a0 phases.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0109_bluenoise_a0_phases || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - bluenoise a0 phases"
    exit 0
}
echo "ok 1 - bluenoise a0 phases"
exit 0
