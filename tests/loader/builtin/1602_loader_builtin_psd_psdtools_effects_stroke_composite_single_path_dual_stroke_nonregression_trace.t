#!/bin/sh
# Verify dual-stroke single-path traces are emitted once per decode path.
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
base_count=0
deferred_count=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

base_count=$(printf '%s\n' "${trace_output}" | awk '
index($0, "builtin PSD: applying single-path dual-stroke blend in layer fallback") {
    c++
}
END {
    print c + 0
}')
deferred_count=$(printf '%s\n' "${trace_output}" | awk '
index($0, "builtin PSD: applying single-path deferred dual-stroke blend on clipped group") {
    c++
}
END {
    print c + 0
}')

test "${base_count}" -eq 1 || {
    echo "not ok" 1 - "effects/stroke-composite base single-path trace count changed"
    exit 0
}

test "${deferred_count}" -eq 1 || {
    echo "not ok" 1 - "effects/stroke-composite deferred single-path trace count changed"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite keeps single-path dual-stroke trace counts stable"
exit 0
