#!/bin/sh
# Verify delegated clbl=1 dual-stroke keeps mode-aware blend on deferred path.
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
command_status=0
delegated_tail=''

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    -Lbuiltin:Eauto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

delegated_tail="${trace_output#*builtin PSD: deferring dual-stroke ownership to clipped group in layer fallback*}"
test "${delegated_tail}" != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missing deferred ownership trace"
    exit 0
}

test "${delegated_tail#*builtin PSD: applying mode-aware dual-stroke blend in layer fallback*}" \
    = "${delegated_tail}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite kept base mode-aware dual-stroke after ownership delegation"
    exit 0
}

test "${delegated_tail#*builtin PSD: applying mode-aware dual-stroke blend on clipped group*}" \
    != "${delegated_tail}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite lost deferred mode-aware dual-stroke after ownership delegation"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite keeps delegated clbl=1 dual-stroke non-regression contract"
exit 0
