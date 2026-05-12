#!/bin/sh
# Verify FXPRI_INSIDE overlap keeps single-pass overlap responsibility.
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
trace_tail=''
diag_line=''
command_status=0
nl='
'

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    -Lbuiltin:Eauto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

diag_line=${trace_output%%"${nl}"*}

test "${diag_line#*FX_DUAL_FXPRI_OVL_DEFER*}" != "${diag_line}" || {
    echo "not ok" 1 - "effects/stroke-composite missing FXPRI overlap code"
    exit 0
}

trace_tail="${trace_output#*builtin PSD: applying effect-priority dual-stroke overlap on clipped group*}"
test "${trace_tail}" != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missing FXPRI overlap trace"
    exit 0
}

test "${trace_tail#*builtin PSD: applying deferred dual-stroke pseudo-layer blend on clipped group*}" \
    != "${trace_tail}" || {
    echo "not ok" 1 - "effects/stroke-composite missing pseudo-layer overlap trace"
    exit 0
}

echo "ok" 1 - "effects/stroke-composite keeps FXPRI overlap single-pass contract"
exit 0
