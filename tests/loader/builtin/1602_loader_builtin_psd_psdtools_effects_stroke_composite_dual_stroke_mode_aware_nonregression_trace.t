#!/bin/sh
# Verify dual-stroke mode-aware traces stay stable and single-path is absent.
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
command_status=0
first_match=''
second_match=''

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

first_match="${trace_output#*builtin PSD: applying mode-aware dual-stroke blend in layer fallback*}"
test "${first_match}" != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missing base mode-aware trace"
    exit 0
}
second_match="${first_match#*builtin PSD: applying mode-aware dual-stroke blend in layer fallback*}"
test "${second_match}" = "${first_match}" || {
    echo "not ok" 1 - "effects/stroke-composite duplicated base mode-aware trace"
    exit 0
}

first_match="${trace_output#*builtin PSD: applying mode-aware dual-stroke blend on clipped group*}"
test "${first_match}" != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missing deferred mode-aware trace"
    exit 0
}
second_match="${first_match#*builtin PSD: applying mode-aware dual-stroke blend on clipped group*}"
test "${second_match}" = "${first_match}" || {
    echo "not ok" 1 - "effects/stroke-composite duplicated deferred mode-aware trace"
    exit 0
}

test "${trace_output#*builtin PSD: applying single-path dual-stroke blend in layer fallback*}" \
    = "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite base single-path trace reappeared"
    exit 0
}

test "${trace_output#*builtin PSD: applying single-path deferred dual-stroke blend on clipped group*}" \
    = "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite deferred single-path trace reappeared"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite keeps mode-aware dual-stroke trace counts stable"
exit 0
