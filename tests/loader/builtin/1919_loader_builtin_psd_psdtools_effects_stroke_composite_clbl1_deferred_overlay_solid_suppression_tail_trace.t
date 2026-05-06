#!/bin/sh
# Verify clbl=1 suppression tail does not reapply base solid overlay.
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
suppressed_tail=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

suppressed_tail="${trace_output#*builtin PSD: suppressing clbl=1 deferred base solid/gradient overlays*}"

test "${suppressed_tail}" != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missing clbl=1 suppression trace"
    exit 0
}

test "${suppressed_tail#*builtin PSD: applying solid overlay effect in layer fallback*}" = "${suppressed_tail}" || {
    echo "not ok" 1 - "effects/stroke-composite reapplied base solid overlay after suppression"
    exit 0
}

echo "ok" 1 - "effects/stroke-composite keeps base solid overlay out of clbl=1 suppression tail"
exit 0
