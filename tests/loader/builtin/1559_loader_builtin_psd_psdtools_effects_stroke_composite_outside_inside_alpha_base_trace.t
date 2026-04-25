#!/bin/sh
# Verify outside fixture keeps base inside stroke alpha semantics.
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
diag_line=''
command_status=0
nl='
'

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    --env SIXEL_PSD_TRACE_HEADER_ONLY=1 \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite outside decode failed"
    exit 0
}

diag_line=${trace_output%%"${nl}"*}
test -n "${diag_line}" || {
    echo "not ok" 1 - "effects/stroke-composite outside missing header line"
    exit 0
}

case "${diag_line}" in
    LSXPSD1\|rc=0\|kind=OK\|codes=*) ;;
    *)
        echo "not ok" 1 - \
            "effects/stroke-composite outside diagnostic header is malformed"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_STROKE_ALPHA_INSIDE_BASE*) ;;
    *)
        echo "not ok" 1 - \
            "effects/stroke-composite outside missing inside base alpha-write"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_STROKE_ALPHA_OUTSIDE_BASE*)
        echo "not ok" 1 - \
            "effects/stroke-composite outside unexpectedly wrote outside base alpha"
        exit 0
        ;;
    *)
        ;;
esac

echo "ok" 1 - "effects/stroke-composite outside keeps inside base alpha-write"
exit 0
