#!/bin/sh
# Policy: docs/loader/libjpeg.md
# Verify independent source-derived values: 12bit-ycck-seq444 profile rejected.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    jpeg-cmyk/precision "${TOP_SRCDIR}" 2 2 3 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required libjpeg precision API or CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - 12bit-ycck-seq444 profile rejected"
    exit 0
}
case "${trace}" in *"JCMYK|target-begin"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - target marker is absent"
    exit 0
}
observed=${trace#*"JCMYK|target-begin"}
case "${observed}" in *"LSXCMS2|event=apply|"*) false ;; *) true ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected CMS apply on the fallback path"
    exit 0
}
echo "ok 1 - 12bit-ycck-seq444 profile rejected"
exit 0
