#!/bin/sh
# Policy: docs/loader/coregraphics.md
# Verify srgb byte with independent source values.
set -eux
echo "1..1"
set -v
result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cg-indexed/0007_srgb_byte 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP CoreGraphics is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - srgb byte values"
    exit 0
}
case "$trace" in *"LSXCMS2|event=apply|engine="*)
    echo "$trace" >&2
    echo "not ok 1 - unexpected CMS on named sRGB byte path"
    exit 0 ;;
esac
echo "ok 1 - srgb byte"
exit 0
