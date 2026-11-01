#!/bin/sh
# Policy: docs/loader/libjpeg.md
# Verify gray12 lcms2 with independent source values.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    jpeg-gray/0005_gray12_lcms2 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required libjpeg precision API or CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - gray12 lcms2 values"
    exit 0
}
case "$trace" in *"event=apply|engine=builtin|"*|*"event=apply|engine=colorsync|"*)
    echo "$trace" >&2
    echo "not ok 1 - unexpected CMS engine"
    exit 0 ;;
esac
case "$trace" in
    *"LSXCMS2|event=apply|engine=lcms2|"*"|src=9|dst=4|ok=1"*) ;;
    *) echo "$trace" >&2; echo "not ok 1 - Gray float apply missing"; exit 0 ;;
esac
echo "ok 1 - gray12 lcms2"
exit 0
