#!/bin/sh
# Policy: docs/loader/coregraphics.md
# Policy: docs/loader/precision-policy.md
# Verify srgb partial composite against independent samples and exact frame metadata.
set -eux
echo "1..1"
set -v
result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cg-expanded/values \
    /tests/data/color-numeric/cg-expanded-srgb-partial.png \
    8 2 1 0 1 0 0 0 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP CoreGraphics or expanded provider is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - srgb partial composite values"
    exit 0
}
trace=${trace##*CGE_FINAL}
case "$trace" in
    *"LSXCMS2|event=apply|engine=colorsync|path=external|src=4|dst=4|ok=1"*) ;;
    *) echo "$trace" >&2; echo "not ok 1 - native RGB apply missing";
       exit 0 ;;
esac
case "$trace" in *"event=apply|engine=builtin|"*|*"event=apply|engine=lcms2|"*)
    echo "$trace" >&2
    echo "not ok 1 - unexpected CMS engine"
    exit 0 ;;
esac
echo "ok 1 - srgb partial composite"
exit 0
