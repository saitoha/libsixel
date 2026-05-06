#!/bin/sh
# Verify clbl=1 deferred solid overlay resolves to replay or zero-coverage skip.
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
apply_tail=''
skip_tail=''
solid_apply_count=0
solid_skip_count=0
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

apply_tail="${suppressed_tail}"
while :
do
    skip_tail="${apply_tail#*builtin PSD: applying clip-weighted deferred solid overlay in layer fallback*}"
    test "${skip_tail}" = "${apply_tail}" && break
    solid_apply_count=$((solid_apply_count + 1))
    apply_tail="${skip_tail}"
done

skip_tail="${suppressed_tail}"
while :
do
    apply_tail="${skip_tail#*builtin PSD: skipping clip-weighted deferred solid overlay in layer fallback due to zero coverage*}"
    test "${apply_tail}" = "${skip_tail}" && break
    solid_skip_count=$((solid_skip_count + 1))
    skip_tail="${apply_tail}"
done

test $((solid_apply_count + solid_skip_count)) -gt 0 || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing deferred solid replay/skip decision trace"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite records deferred solid replay/skip decision"
exit 0
