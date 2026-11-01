#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify lab linear infinite result.
set -eux
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0351_lab_linear_infinite_result || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "not ok 1 - lab linear infinite result"
    exit 0
}
echo "ok 1 - lab linear infinite result"
exit 0
