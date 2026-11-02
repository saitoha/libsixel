#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify clbl=1 deferred overlay suppression keeps tail ordering consistent.
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

# Inspect one line at a time to avoid shell glob limits on full traces.

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite.psd"
trace_output=''
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


suppressed=0
replayed=0
while IFS= read -r line; do
    case "$line" in
    *"builtin PSD: suppressing clbl=1 deferred base solid/gradient overlays"*)
        suppressed=1
        ;;
    *"builtin PSD: applying clip-weighted deferred gradient overlay in layer fallback"*)
        test "$suppressed" -eq 0 || replayed=1
        ;;
    *"builtin PSD: applying solid overlay effect in layer fallback"*)
        test "$suppressed" -eq 0 || {
            echo "not ok 1 - base solid overlay followed suppression"
            exit 0
        }
        ;;
    esac
done <<EOF
$trace_output
EOF
test "$replayed" -eq 1 || {
    echo "not ok 1 - deferred gradient replay did not follow suppression"
    exit 0
}

echo "ok" 1 - "effects/stroke-composite keeps suppression tail consistent with deferred gradient overlay"
exit 0
