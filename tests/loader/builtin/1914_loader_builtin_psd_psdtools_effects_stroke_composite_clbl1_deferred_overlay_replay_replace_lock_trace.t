#!/bin/sh
# Verify replay replacement/locked-skip traces do not appear after commit.
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
committed_tail=''
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

committed_tail="${trace_output##*builtin PSD: compositing deferred offscreen clipped group buffer to canvas*}"

test "${committed_tail}" != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missing deferred group commit trace"
    exit 0
}

test "${committed_tail#*builtin PSD: replacing deferred clbl=1 overlay replay entry in layer fallback*}" \
    = "${committed_tail}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite replay replacement trace reappeared after commit"
    exit 0
}

test "${committed_tail#*builtin PSD: skipping deferred overlay replay enqueue while group replay is locked*}" \
    = "${committed_tail}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite replay lock skip trace reappeared after commit"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite keeps replay replacement/skip traces out of post-commit tail"
exit 0
