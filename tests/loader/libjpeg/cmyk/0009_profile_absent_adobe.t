#!/bin/sh
# Policy: docs/loader/libjpeg.md
# Verify profile absent adobe.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    jpeg-cmyk/0009_profile_absent_adobe "${TOP_SRCDIR}" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required libjpeg or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - profile absent adobe"
    exit 0
}
test "${trace#*"JCMYK|target-begin"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - target observation marker is absent"
    exit 0
}
observed=${trace#*"JCMYK|target-begin"}
test "$observed" = "${observed#*"LSXCMS2|event=apply"}" || {
    echo "$trace" >&2
    echo "not ok 1 - fallback unexpectedly applied an ICC transform"
    exit 0
}
echo "ok 1 - profile absent adobe"
exit 0
