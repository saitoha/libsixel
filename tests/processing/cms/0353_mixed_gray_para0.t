#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify mixed gray para0.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0353_mixed_gray_para0 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP ColorSync is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - mixed gray para0"
    exit 0
}
actual=${trace#*'MDPHASE|begin
'}
actual=${actual%%'MDPHASE|end'*}
expected=${trace#*'MDEXPECT1|begin
'}
expected=${expected%%'MDEXPECT1|end'*}
test "${actual#*"$expected"}" != "$actual" || {
    echo "$trace" >&2
    echo "not ok 1 - first native group payload differs"
    exit 0
}
expected=${trace#*'MDEXPECT2|begin
'}
expected=${expected%%'MDEXPECT2|end'*}
test "${actual#*"$expected"}" != "$actual" || {
    echo "$trace" >&2
    echo "not ok 1 - second native group payload differs"
    exit 0
}
remaining=${actual#*'LSXCMS2|event=apply|engine=colorsync|'}
test "$remaining" != "$actual" || {
    echo "$trace" >&2
    echo "not ok 1 - expected native apply is absent"
    exit 0
}
actual=$remaining
remaining=${actual#*'LSXCMS2|event=apply|engine=colorsync|'}
test "$remaining" != "$actual" || {
    echo "$trace" >&2
    echo "not ok 1 - expected native apply is absent"
    exit 0
}
actual=$remaining
test "${actual#*'LSXCMS2|event=apply|'}" = "$actual" || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected additional CMS apply"
    exit 0
}
echo "ok 1 - mixed gray para0"
exit 0
