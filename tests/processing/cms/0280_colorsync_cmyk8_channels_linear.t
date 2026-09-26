#!/bin/sh
# Policy: docs/loader/color-management/README.md
# independent positive values and prewrite channel rejection
set -eux
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0280_colorsync_cmyk8_channels_linear 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP ColorSync or required loader unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - independent positive values and prewrite channel rejection"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=colorsync|path=external|src=5|dst=4|ok=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - missing expected contract trace"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=colorsync|path=external|src=5|dst=4|ok=0"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - missing expected contract trace"
    exit 0
}
case "${trace}" in *"LSXCMS1|intent_order=relative|exclusive=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - missing expected contract trace"
    exit 0
}
case "${trace}" in *"independent-values-and-prewrite-guards-verified"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - missing expected contract trace"
    exit 0
}
echo "ok 1 - 0280_colorsync_cmyk8_channels_linear independent positive values and prewrite channel rejection"
exit 0
