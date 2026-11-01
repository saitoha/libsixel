#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify gray rejected against analytic JPEG samples.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    bjicc/0049_bj_gray_rejected_numeric 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP requested CMS backend is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - gray rejected values"
    exit 0
}
case "$trace" in *"LSXCMS2|event=apply|"*)
    echo "$trace" >&2
    echo "not ok 1 - unexpected CMS apply"
    exit 0 ;;
esac
echo "ok 1 - gray rejected"
exit 0
