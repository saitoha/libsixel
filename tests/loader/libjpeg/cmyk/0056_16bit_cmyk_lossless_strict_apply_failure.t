#!/bin/sh
# Policy: docs/loader/libjpeg.md
# Verify independent source-derived values: 16bit-cmyk-lossless strict apply failure.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    jpeg-cmyk/precision "${TOP_SRCDIR}" 3 2 4 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required libjpeg precision API or CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - 16bit-cmyk-lossless strict apply failure"
    exit 0
}
case "${trace}" in *"JCMYK|target-begin"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - target marker is absent"
    exit 0
}
observed=${trace#*"JCMYK|target-begin"}
case "${observed}" in *"LSXCMS2|event=apply|engine=builtin|path="*"|src=6|dst=4|ok=0"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - selected CMS apply outcome is absent"
    exit 0
}
control=${trace%%"JCMYK|target-begin"*}
case "${control}" in *"LSXCMS2|event=apply|engine=builtin|"*"|ok=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - successful transform qualification is absent"
    exit 0
}
echo "ok 1 - 16bit-cmyk-lossless strict apply failure"
exit 0
