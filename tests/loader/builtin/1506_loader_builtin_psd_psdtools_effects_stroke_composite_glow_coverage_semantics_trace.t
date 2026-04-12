#!/bin/sh
# Verify stroke-composite applies non-default glow coverage semantics.
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

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite.psd"
trace_output=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: parsed OrGl glow source/choke/range semantics*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missed OrGl source/choke/range parse semantics"
    exit 0
}

test "${trace_output#*builtin PSD: parsed IrGl glow source/choke/range semantics*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missed IrGl source/choke/range parse semantics"
    exit 0
}

test "${trace_output#*builtin PSD: clbl=1; deferring interior overlays to clipped group composite*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite lost clbl=1 deferred contract"
    exit 0
}

test "${trace_output#*builtin PSD: suppressing clbl=1 deferred base interior glow/choke/bevel-shadow*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missed deferred glow/choke suppression contract"
    exit 0
}

test "${trace_output#*builtin PSD: applying clip-weighted deferred interior effects in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missed deferred interior apply path"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite keeps glow semantics and deferred interior contracts"
exit 0
