#!/bin/sh
# Verify stroke-composite center fixture keeps deferred center stroke coverage.
# Fixture derivation command:
#   python3 - <<'PY'
#   from pathlib import Path
#   src = Path("tests/data/psd-tools/psdtools_effects_stroke_composite.psd")
#   dst = Path("tests/data/psd-tools/psdtools_effects_stroke_composite_center.psd")
#   dst.write_bytes(src.read_bytes().replace(b"InsF", b"CtrF"))
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

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite_center.psd"
trace_output=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite center decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: applying distance-map deferred effect stroke coverage in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite center missing deferred distance-map stroke path"
    exit 0
}

test "${trace_output#*builtin PSD: using distance-map deferred center stroke coverage in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite center missing deferred center stroke semantics"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite center keeps deferred center stroke semantics"
exit 0
