#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify managed rgb late builtin with independent values or qualified failures.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0263_managed_rgb_late_builtin \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-managed-rgb.tif" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff or requested CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - managed rgb late builtin"
    exit 0
}
observed=${trace#*"TMFAULT|phase-begin=1"}
observed=${observed%%"TMFAULT|phase-begin=2"*}
test "${observed#*"LSXCMS2|event=apply|engine=builtin|"}" != "$observed" || {
    echo "$trace" >&2
    echo "not ok 1 - requested engine did not apply"
    exit 0
}
test "${observed#*"|ok=0"}" != "$observed" || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected native apply result"
    exit 0
}
echo "ok 1 - managed rgb late builtin"
exit 0
