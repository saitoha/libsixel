#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify delegated dual-stroke ownership/apply traces are emitted once.
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
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:Eauto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

case "${trace_output}" in *builtin\ PSD:\ deferring\ dual-stroke\ ownership\ to\ clipped\ group\ in\ layer\ fallback**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "effects/stroke-composite missing deferred ownership trace"
    exit 0
}
case "${trace_output}" in *"builtin PSD: deferring dual-stroke ownership to clipped group in layer fallback"*builtin\ PSD:\ deferring\ dual-stroke\ ownership\ to\ clipped\ group\ in\ layer\ fallback**) false ;; *) true ;; esac || {
    echo "not ok" 1 - "effects/stroke-composite duplicated deferred ownership trace"
    exit 0
}

case "${trace_output}" in *builtin\ PSD:\ applying\ owned\ deferred\ dual-stroke\ on\ clipped\ group**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "effects/stroke-composite missing owned deferred apply trace"
    exit 0
}
case "${trace_output}" in *"builtin PSD: applying owned deferred dual-stroke on clipped group"*builtin\ PSD:\ applying\ owned\ deferred\ dual-stroke\ on\ clipped\ group**) false ;; *) true ;; esac || {
    echo "not ok" 1 - "effects/stroke-composite duplicated owned deferred apply trace"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite keeps deferred dual-stroke ownership/apply counts stable"
exit 0
