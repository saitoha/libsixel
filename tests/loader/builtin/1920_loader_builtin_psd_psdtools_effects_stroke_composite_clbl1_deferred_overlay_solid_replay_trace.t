#!/bin/sh
# Verify clbl=1 deferred ownership records a solid replay/skip decision code
# after base suppression.
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
solid_decision_count=0
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

test "${diag_line#*FX_CLBL1_BASE_OVERLAY_SUPPRESS*}" != "${diag_line}" || {
    echo "not ok" 1 - "effects/stroke-composite missing clbl=1 suppression code"
    exit 0
}

solid_apply_tail="${diag_line#*FX_DEFERRED_SOLID_OVERLAY_CLIP*}"
test "${solid_apply_tail}" = "${diag_line}" || \
    solid_decision_count=$((solid_decision_count + 1))

solid_skip_zero_tail="${diag_line#*FX_DEFERRED_SOLID_SKIP_ZERO_COVERAGE*}"
test "${solid_skip_zero_tail}" = "${diag_line}" || \
    solid_decision_count=$((solid_decision_count + 1))

solid_skip_unsuppressed_tail="${diag_line#*FX_DEFERRED_SOLID_SKIP_UNSUPPRESSED*}"
test "${solid_skip_unsuppressed_tail}" = "${diag_line}" || \
    solid_decision_count=$((solid_decision_count + 1))

test "${solid_decision_count}" -gt 0 || {
    echo "not ok" 1 - "effects/stroke-composite missing deferred solid replay/skip decision trace"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite records deferred solid replay/skip decision after clbl=1 suppression"
exit 0
