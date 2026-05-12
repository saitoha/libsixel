#!/bin/sh
# Verify clbl=1 delegated dual-stroke applies once on the deferred pass.
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
first_match=''
second_match=''
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

first_match="${trace_output#*builtin PSD: applying owned deferred dual-stroke on clipped group*}"
test "${first_match}" != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing owned deferred dual-stroke apply trace"
    exit 0
}
second_match="${first_match#*builtin PSD: applying owned deferred dual-stroke on clipped group*}"
test "${second_match}" = "${first_match}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite duplicated owned deferred dual-stroke apply trace"
    exit 0
}

first_match="${trace_output#*builtin PSD: applying deferred stroke on clipped group*}"
test "${first_match}" != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missing deferred stroke trace"
    exit 0
}
second_match="${first_match#*builtin PSD: applying deferred stroke on clipped group*}"
test "${second_match}" = "${first_match}" || {
    echo "not ok" 1 - "effects/stroke-composite duplicated deferred stroke trace"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite keeps clbl=1 deferred dual-stroke single-dispatch"
exit 0
