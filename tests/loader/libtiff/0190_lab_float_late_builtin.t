#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify lab float late builtin.
set -eux
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0190_lab_float_late_builtin \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-lab-float-521.tif" || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "not ok 1 - lab float late builtin"
    exit 0
}
echo "ok 1 - lab float late builtin"
exit 0
