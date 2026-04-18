#!/bin/sh
# Verify stroke-composite outside fixture uses base outside distance-map stroke.
# Fixture derivation command:
#   python3 - <<'PY'
#   from pathlib import Path
#   src = Path("tests/data/psd-tools/psdtools_effects_stroke_composite.psd")
#   dst = Path("tests/data/psd-tools/psdtools_effects_stroke_composite_outside.psd")
#   dst.write_bytes(src.read_bytes().replace(b"InsF", b"OutF"))
#   PY

set -eux

: "${IMG2SIXEL_PATH:=${TOP_BUILDDIR}/converters/img2sixel}"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite_outside.psd"
trace_output=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite outside decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: applying distance-map effect stroke coverage in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite outside missing base distance-map stroke path"
    exit 0
}

test "${trace_output#*builtin PSD: using distance-map outside stroke coverage in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite outside missing base outside stroke semantics"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite outside keeps base outside stroke semantics"
exit 0
