#!/bin/sh
# Verify legacy colorsync rejection.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    jpeg-cmyk/0019_legacy_colorsync_rejection "${TOP_SRCDIR}" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required libjpeg or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - legacy colorsync rejection"
    exit 0
}
test "${trace#*"JCMYK|target-begin"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - target observation marker is absent"
    exit 0
}
observed=${trace#*"JCMYK|target-begin"}
control=${trace%%"JCMYK|target-begin"*}
case "$control" in
    *"LSXCMS2|event=apply|engine=colorsync|"*"|ok=1"*) ;;
    *) echo "$trace" >&2; echo "not ok 1 - successful CMS qualification is absent"; exit 0 ;;
esac
test "$observed" = "${observed#*"LSXCMS2|event=apply"}" || {
    echo "$trace" >&2
    echo "not ok 1 - fallback unexpectedly applied an ICC transform"
    exit 0
}
echo "ok 1 - legacy colorsync rejection"
exit 0
