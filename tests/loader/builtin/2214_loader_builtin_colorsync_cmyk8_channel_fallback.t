#!/bin/sh
# Policy: docs/loader/color-management.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# original-input CMYK8 fallback values and frame format
set -eux
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0198_colorsync_cmyk8_channel_fallback "${TOP_SRCDIR}/tests/data/color-numeric/colorsync-channel-cmyk8-none.psd" "${TOP_SRCDIR}/tests/data/color-numeric/colorsync-channel-cmyk8-mismatch.psd" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP ColorSync or required loader unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - original-input CMYK8 fallback values and frame format"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|engine=colorsync|path=external|src=5|dst=4|ok=0"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - missing expected contract trace"
    exit 0
}
test "${trace#*"CSCHFRAME|depth=8|original-input-fallback-values-and-format-verified"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - missing expected contract trace"
    exit 0
}
echo "ok 1 - 0198_colorsync_cmyk8_channel_fallback original-input CMYK8 fallback values and frame format"
exit 0
