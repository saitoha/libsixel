#!/bin/sh
# Verify refine initially unneeded.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/palette/0126_refine_initially_unneeded" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - refine initially unneeded"
    exit 0
}
echo "ok 1 - refine initially unneeded"
exit 0
