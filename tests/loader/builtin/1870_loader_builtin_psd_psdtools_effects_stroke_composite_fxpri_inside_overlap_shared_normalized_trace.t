#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify FXPRI_INSIDE overlap runs in deferred overlap decomposition flow.
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

# Match ordered markers directly; shortest-prefix glob removal is quadratic.

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite.psd"
trace_output=''
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

case "${diag_line}" in *FX_DUAL_OVERLAP_DEFER**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "effects/stroke-composite missing deferred overlap code"
    exit 0
}

case "${diag_line}" in *FX_DUAL_FXPRI_OVL_DEFER**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "effects/stroke-composite missing FXPRI overlap code"
    exit 0
}

case "${trace_output}" in *builtin\ PSD:\ applying\ deferred\ dual-stroke\ overlap\ decomposition\ on\ clipped\ group**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "effects/stroke-composite missing deferred overlap decomposition trace"
    exit 0
}

case "${trace_output}" in *"builtin PSD: applying deferred dual-stroke overlap decomposition on clipped group"*builtin\ PSD:\ applying\ effect-priority\ dual-stroke\ overlap\ on\ clipped\ group**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "effects/stroke-composite missing FXPRI overlap trace"
    exit 0
}

echo "ok" 1 - "effects/stroke-composite keeps FXPRI overlap in deferred overlap flow"
exit 0
