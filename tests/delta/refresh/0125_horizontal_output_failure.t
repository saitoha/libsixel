#!/bin/sh
# Verify horizontal output failure.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0125_horizontal_output_failure || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - horizontal output failure"
    exit 0
}
echo "ok 1 - horizontal output failure"
exit 0
