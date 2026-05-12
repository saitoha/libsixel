#!/bin/sh
# Verify clbl=1 deferred overlays are applied before offscreen-group commit.
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
deferred_tail=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    -Lbuiltin:Eauto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: clbl=1; deferring interior overlays to clipped group composite*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing clbl=1 deferred-overlay routing trace"
    exit 0
}

deferred_tail="${trace_output#*builtin PSD: applying clip-weighted deferred gradient overlay in layer fallback*}"
test "${deferred_tail}" != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing deferred gradient overlay apply trace"
    exit 0
}

test "${deferred_tail#*builtin PSD: compositing deferred offscreen clipped group buffer to canvas*}" \
    != "${deferred_tail}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing offscreen-group commit after deferred overlays"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite keeps deferred overlays in offscreen group before commit"
exit 0
