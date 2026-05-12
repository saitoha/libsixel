#!/bin/sh
# Verify clbl=1 deferred replay keeps base SoFi when replay replacement is
# absent in the current fixture.
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

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite.psd"
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
    -Lbuiltin:Eauto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

diag_line=${trace_output#*LSXPSD1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missing LSXPSD1 contract header"
    exit 0
}

diag_line="LSXPSD1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#*FX_DEFERRED_OVERLAY_REPLAY_REPLACE*}" = "${diag_line}" || {
    echo "not ok" 1 - "effects/stroke-composite emitted deferred replay replacement code"
    exit 0
}

test "${diag_line#*FX_SOLID_OVERLAY_BASE*}" != "${diag_line}" || {
    echo "not ok" 1 - "effects/stroke-composite missing base solid overlay code"
    exit 0
}

echo "ok" 1 - "effects/stroke-composite keeps base SoFi when deferred replay source is unchanged"
exit 0
