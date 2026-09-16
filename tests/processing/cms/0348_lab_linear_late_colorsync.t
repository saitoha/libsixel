#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify lab linear late colorsync.
set -eux
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0348_lab_linear_late_colorsync || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "not ok 1 - lab linear late colorsync"
    exit 0
}
echo "ok 1 - lab linear late colorsync"
exit 0
