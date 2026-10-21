#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
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

# PSD emits one trace event per line. Count those events in a single pass
# instead of repeatedly searching every prefix of the entire trace.
while IFS= read -r trace_line
do
    case "$trace_line" in
        *"builtin PSD: suppressing clbl=1 deferred base solid/gradient overlays"*)
            suppression_count=$((suppression_count + 1)) ;;
        *"builtin PSD: replaying deferred clbl=1 overlay entry in layer fallback"*)
            replay_count=$((replay_count + 1)) ;;
    esac
done <<EOF
${trace_output}
EOF

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
