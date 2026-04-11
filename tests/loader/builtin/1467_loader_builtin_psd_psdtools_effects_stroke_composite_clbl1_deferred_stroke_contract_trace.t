#!/bin/sh
# Verify clbl=1 deferred stroke contract remains intact while clip-weighted
# deferred passes are active on effects/stroke-composite.
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
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite.psd"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"
trace_output=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:e=auto! -o "${output_sixel}" "${input_psd}" 2>&1) || command_status=$?
: "${trace_output}"

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: clbl=1; deferring interior overlays to clipped group composite*}" != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite did not defer clbl=1 interior overlays"
    exit 0
}

test "${trace_output#*builtin PSD: applying deferred stroke on clipped group*}" != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite did not keep deferred stroke"
    exit 0
}

test "${trace_output#*builtin PSD: suppressing synthesized vector stroke on clipping-group base layer*}" = "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite unexpectedly suppressed clbl=1 vector stroke"
    exit 0
}

echo "ok" 1 - "effects/stroke-composite keeps clbl=1 deferred-stroke contract"
exit 0
