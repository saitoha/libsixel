#!/bin/sh
# Verify clbl=1 deferred solid replay is emitted when suppression is adopted
# for the selected replay source layer.
# Fixture/expected regeneration command:
#   python3 tests/data/psd-tools/generate_psdtools_hybrid_assets.py --download

set -eux

: "${IMG2SIXEL_PATH:=${TOP_BUILDDIR}/converters/img2sixel}"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_blend_and_clipping.psd"
trace_output=''
diag_line=''
command_status=0
nl='
'

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --lookup-policy=none \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    --env SIXEL_PSD_TRACE_HEADER_ONLY=1 \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "blend_and_clipping decode failed"
    exit 0
}

diag_line=${trace_output#*LSXPSD1|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "blend_and_clipping missing LSXPSD1 contract header"
    exit 0
}

diag_line="LSXPSD1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#*FX_SOLID_OVERLAY_BASE*}" != "${diag_line}" || {
    echo "not ok" 1 - "blend_and_clipping missing base solid overlay code"
    exit 0
}

test "${diag_line#*FX_DEFERRED_SOLID_OVERLAY_CLIP*}" != "${diag_line}" || {
    echo "not ok" 1 - \
        "blend_and_clipping missing deferred solid replay code"
    exit 0
}

test "${diag_line#*FX_DEFERRED_SOLID_SKIP_UNSUPPRESSED*}" = "${diag_line}" || {
    echo "not ok" 1 - \
        "blend_and_clipping emitted deferred solid unsuppressed-skip code"
    exit 0
}

echo "ok" 1 - \
    "blend_and_clipping emits clbl=1 deferred solid replay when suppression is adopted"
exit 0
