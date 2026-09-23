#!/bin/sh
# Policy: docs/loader/coregraphics.md
# Verify p3 reqcolors with independent source values.
set -eux
echo "1..1"
set -v
result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cg-indexed/0006_p3_reqcolors 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP CoreGraphics is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - p3 reqcolors values"
    exit 0
}
case "$trace" in
    *"LSXCMS2|event=apply|engine=colorsync|path=external|src=4|dst=4|ok=1"*) ;;
    *) echo "$trace" >&2; echo "not ok 1 - native RGB apply missing";
       exit 0 ;;
esac
case "$trace" in *"event=apply|engine=builtin|"*|*"event=apply|engine=lcms2|"*)
    echo "$trace" >&2
    echo "not ok 1 - unexpected CMS engine"
    exit 0 ;;
esac
echo "ok 1 - p3 reqcolors"
exit 0
