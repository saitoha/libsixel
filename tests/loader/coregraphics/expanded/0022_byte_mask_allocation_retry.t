#!/bin/sh
# Policy: docs/loader/coregraphics.md
# Policy: docs/loader/precision-policy.md
# Verify byte mask allocation retry against independent samples and exact frame metadata.
set -eux
echo "1..1"
set -v
result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cg-expanded/values \
    /tests/data/color-numeric/cg-expanded-srgb-partial.png \
    8 2 1 0 0 1 2 0 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP CoreGraphics or expanded provider is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - byte mask allocation retry values"
    exit 0
}
trace=${trace##*CGE_FINAL}
case "$trace" in *"event=apply|"*)
    echo "$trace" >&2
    echo "not ok 1 - unexpected CMS on byte path"
    exit 0 ;;
esac
echo "ok 1 - byte mask allocation retry"
exit 0
