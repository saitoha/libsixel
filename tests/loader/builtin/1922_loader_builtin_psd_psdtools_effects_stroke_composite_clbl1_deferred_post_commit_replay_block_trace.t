#!/bin/sh
# Verify deferred apply traces do not reappear after offscreen group commit.
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

test "${committed_tail#*builtin PSD: replaying deferred clbl=1 overlay entry in layer fallback*}" \
    = "${committed_tail}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite replayed deferred overlay after group commit"
    exit 0
}

test "${committed_tail#*builtin PSD: applying clip-weighted deferred solid overlay in layer fallback*}" \
    = "${committed_tail}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite applied deferred solid overlay after group commit"
    exit 0
}

test "${committed_tail#*builtin PSD: applying clip-weighted deferred gradient overlay in layer fallback*}" \
    = "${committed_tail}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite applied deferred gradient overlay after group commit"
    exit 0
}

test "${committed_tail#*builtin PSD: applying deferred stroke on clipped group*}" \
    = "${committed_tail}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite applied deferred stroke after group commit"
    exit 0
}

test "${committed_tail#*builtin PSD: skipping clip-weighted deferred solid overlay in layer fallback due to zero coverage*}" \
    = "${committed_tail}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite retried deferred solid skip after group commit"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite blocks deferred replay traces in post-commit tail"
exit 0
