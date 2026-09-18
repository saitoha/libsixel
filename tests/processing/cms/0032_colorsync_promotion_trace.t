#!/bin/sh
# Policy: docs/loader/color-management.md
# A normalized float source is an explicit ColorSync adapter path.
set -eux
echo "1..1"
set -v

status=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "cms/0025_integer_rgb_float" colorsync 4 separate 2>&1) || status=$?
test "$status" -ne 77 || {
    echo "ok 1 - ColorSync promotion trace # SKIP ColorSync unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - ColorSync integer conversion"
    exit 0
}
case "${trace}" in *"event=source-promotion|engine=colorsync|src=3|dst=4|via=float32"*) true ;; *) false ;; esac || {
    echo "not ok 1 - ColorSync source promotion trace missing"
    exit 0
}
case "${trace}" in *"event=apply|engine=colorsync|path=external|src=3|dst=4|ok=1"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual ColorSync apply trace missing"
    exit 0
}
echo "ok 1 - ColorSync source promotion and actual apply trace"
exit 0
