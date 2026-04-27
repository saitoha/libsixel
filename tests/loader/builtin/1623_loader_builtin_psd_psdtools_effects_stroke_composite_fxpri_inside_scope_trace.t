#!/bin/sh
# Verify FXPRI inside overlap policy is scoped to same-mode inside dual stroke.
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

inside_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite.psd"
outside_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite_outside.psd"
inside_trace=''
outside_trace=''
inside_diag=''
outside_diag=''
inside_status=0
outside_status=0
nl='
'

inside_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    --env SIXEL_PSD_TRACE_HEADER_ONLY=1 \
    -Lbuiltin:e=auto! -o /dev/null "${inside_psd}" 2>&1) || \
    inside_status=$?

test "${inside_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite inside decode failed"
    exit 0
}

outside_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    --env SIXEL_PSD_TRACE_HEADER_ONLY=1 \
    -Lbuiltin:e=auto! -o /dev/null "${outside_psd}" 2>&1) || \
    outside_status=$?

test "${outside_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite outside decode failed"
    exit 0
}

inside_diag=${inside_trace%%"${nl}"*}
outside_diag=${outside_trace%%"${nl}"*}

test "${inside_diag#*FX_DUAL_FXPRI_OVL_DEFER*}" != "${inside_diag}" || {
    echo "not ok" 1 - "effects/stroke-composite inside missing FXPRI code"
    exit 0
}

test "${outside_diag#*FX_DUAL_FXPRI_OVL_DEFER*}" = "${outside_diag}" || {
    echo "not ok" 1 - "effects/stroke-composite outside unexpectedly used FXPRI code"
    exit 0
}

echo "ok" 1 - "effects/stroke-composite scopes FXPRI overlap policy to inside dual stroke"
exit 0
