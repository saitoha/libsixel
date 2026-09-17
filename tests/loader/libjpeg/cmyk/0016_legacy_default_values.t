#!/bin/sh
# Verify legacy default values.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    jpeg-cmyk/0016_legacy_default_values "${TOP_SRCDIR}" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required libjpeg or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - legacy default values"
    exit 0
}
test "${trace#*"JCMYK|target-begin"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - target observation marker is absent"
    exit 0
}
observed=${trace#*"JCMYK|target-begin"}
case "$observed" in
    *"LSXCMS2|event=apply|engine=builtin|"*"|src=5|dst=4|ok=1"*) ;;
    *) echo "$trace" >&2; echo "not ok 1 - actual CMS apply outcome is absent"; exit 0 ;;
esac
echo "ok 1 - legacy default values"
exit 0
