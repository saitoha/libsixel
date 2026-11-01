#!/bin/sh
# Policy: docs/loader/libjpeg.md
# Verify independent source-derived values: 12bit-ycck-seq444 lcms2.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    jpeg-cmyk/precision "${TOP_SRCDIR}" 2 3 0 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required libjpeg precision API or CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - 12bit-ycck-seq444 lcms2"
    exit 0
}
case "${trace}" in *"JCMYK|target-begin"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - target marker is absent"
    exit 0
}
observed=${trace#*"JCMYK|target-begin"}
case "${observed}" in *"LSXCMS2|event=apply|engine=lcms2|path="*"|src=6|dst=4|ok=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - selected CMS apply outcome is absent"
    exit 0
}
echo "ok 1 - 12bit-ycck-seq444 lcms2"
exit 0
