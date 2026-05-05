#!/bin/sh
# Verify clbl=1 deferred overlay replay count matches suppression count.
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
suppression_count=0
replay_count=0
suppression_tail=''
replay_tail=''
next_tail=''
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

suppression_tail="${trace_output}"
while :
do
    next_tail="${suppression_tail#*builtin PSD: suppressing clbl=1 deferred base solid/gradient overlays*}"
    test "${next_tail}" = "${suppression_tail}" && break
    suppression_count=$((suppression_count + 1))
    suppression_tail="${next_tail}"
done

replay_tail="${trace_output}"
while :
do
    next_tail="${replay_tail#*builtin PSD: replaying deferred clbl=1 overlay entry in layer fallback*}"
    test "${next_tail}" = "${replay_tail}" && break
    replay_count=$((replay_count + 1))
    replay_tail="${next_tail}"
done

test "${suppression_count}" -gt 0 || {
    echo "not ok" 1 - "effects/stroke-composite missing clbl=1 suppression trace"
    exit 0
}

test "${suppression_count}" -eq "${replay_count}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite deferred replay count does not match suppression count"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite keeps clbl=1 deferred overlay replay count aligned"
exit 0
