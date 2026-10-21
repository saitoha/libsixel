#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify clbl=1 deferred group commits offscreen output to canvas once.
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
    --env SIXEL_PSD_TRACE_ONLY=1 \
    -Lbuiltin:Eauto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

case "${trace_output}" in *builtin\ PSD:\ compositing\ deferred\ offscreen\ clipped\ group\ buffer\ to\ canvas**) true ;; *) false ;; esac || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing deferred offscreen-group commit trace"
    exit 0
}

case "${trace_output}" in *"builtin PSD: compositing deferred offscreen clipped group buffer to canvas"*builtin\ PSD:\ compositing\ deferred\ offscreen\ clipped\ group\ buffer\ to\ canvas**) false ;; *) true ;; esac || {
    echo "not ok" 1 - \
        "effects/stroke-composite duplicated deferred offscreen-group commit trace"
    exit 0
}

case "${trace_output}" in *builtin\ PSD:\ applying\ deferred\ stroke\ on\ clipped\ group**) true ;; *) false ;; esac || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing deferred stroke apply trace before commit"
    exit 0
}

case "${trace_output}" in *"builtin PSD: applying deferred stroke on clipped group"*builtin\ PSD:\ compositing\ deferred\ offscreen\ clipped\ group\ buffer\ to\ canvas**) true ;; *) false ;; esac || {
    echo "not ok" 1 - \
        "effects/stroke-composite did not commit offscreen group after deferred stroke"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite keeps one offscreen-group commit per deferred pass"
exit 0
