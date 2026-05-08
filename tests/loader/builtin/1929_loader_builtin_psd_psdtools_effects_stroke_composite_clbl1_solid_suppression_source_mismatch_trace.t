#!/bin/sh
# Verify clbl=1 deferred replay keeps base SoFi outside suppression tail when
# the adopted solid replay source does not require local base suppression.
# Fixture/expected regeneration command:
#   python3 tests/data/psd-tools/generate_psdtools_hybrid_assets.py --download

set -eux

IMG2SIXEL_PATH="${TOP_BUILDDIR}/converters/img2sixel"

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
suppressed_tail=''
command_status=0
nl='
'

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --lookup-policy=none \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

diag_line=${trace_output#*LSXPSD1|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missing LSXPSD1 contract header"
    exit 0
}

diag_line="LSXPSD1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#*FX_CLBL1_BASE_OVERLAY_SUPPRESS*}" != "${diag_line}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing clbl=1 base overlay suppression code"
    exit 0
}

test "${diag_line#*FX_DEFERRED_SOLID_OVERLAY_CLIP*}" != "${diag_line}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing deferred solid replay code"
    exit 0
}

test "${trace_output#*builtin PSD: applying solid overlay effect in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing global base solid apply trace"
    exit 0
}

suppressed_tail="${trace_output#*builtin PSD: suppressing clbl=1 deferred base solid/gradient overlays*}"

test "${suppressed_tail}" != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing clbl=1 suppression trace"
    exit 0
}

test "${suppressed_tail#*builtin PSD: applying clip-weighted deferred solid overlay in layer fallback*}" \
    != "${suppressed_tail}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing deferred solid replay in suppression tail"
    exit 0
}

test "${suppressed_tail#*builtin PSD: applying solid overlay effect in layer fallback*}" \
    = "${suppressed_tail}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite found base solid apply in suppression tail"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite keeps clbl=1 solid suppression local to replay-adopted layer"
exit 0
