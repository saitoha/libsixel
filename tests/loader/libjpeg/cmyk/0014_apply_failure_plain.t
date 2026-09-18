#!/bin/sh
# Verify apply failure plain.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    jpeg-cmyk/0014_apply_failure_plain "${TOP_SRCDIR}" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required libjpeg or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - apply failure plain"
    exit 0
}
case "${trace}" in *"JCMYK|target-begin"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - target observation marker is absent"
    exit 0
}
observed=${trace#*"JCMYK|target-begin"}
control=${trace%%"JCMYK|target-begin"*}
case "$control" in
    *"LSXCMS2|event=apply|engine=builtin|"*"|ok=1"*) ;;
    *) echo "$trace" >&2; echo "not ok 1 - successful CMS qualification is absent"; exit 0 ;;
esac
case "$observed" in
    *"LSXCMS2|event=apply|engine=builtin|"*"|src=5|dst=4|ok=0"*) ;;
    *) echo "$trace" >&2; echo "not ok 1 - actual CMS apply outcome is absent"; exit 0 ;;
esac
echo "ok 1 - apply failure plain"
exit 0
