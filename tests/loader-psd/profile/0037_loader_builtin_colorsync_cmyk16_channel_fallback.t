#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# original-input CMYK16 fallback values and frame format
set -eux
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0199_colorsync_cmyk16_channel_fallback "${TOP_SRCDIR}/tests/data/color-numeric/colorsync-channel-cmyk16-none.psd" "${TOP_SRCDIR}/tests/data/color-numeric/colorsync-channel-cmyk16-mismatch.psd" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP ColorSync or required loader unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - original-input CMYK16 fallback values and frame format"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=colorsync|path=external|src=6|dst=4|ok=0"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - missing expected contract trace"
    exit 0
}
case "${trace}" in *"CSCHFRAME|depth=16|original-input-fallback-values-and-format-verified"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - missing expected contract trace"
    exit 0
}
echo "ok 1 - 0199_colorsync_cmyk16_channel_fallback original-input CMYK16 fallback values and frame format"
exit 0
