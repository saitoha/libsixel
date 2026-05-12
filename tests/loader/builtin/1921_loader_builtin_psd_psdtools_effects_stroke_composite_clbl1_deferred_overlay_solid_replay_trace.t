#!/bin/sh
# Verify clbl=1 deferred solid overlay emits exactly one replay/skip decision
# code in the contract header.
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
diag_line=''
solid_apply_count=0
solid_skip_count=0
solid_unsuppressed_skip_count=0
command_status=0
nl='
'

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --lookup-policy=none \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    --env SIXEL_PSD_TRACE_HEADER_ONLY=1 \
    -Lbuiltin:Eauto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

diag_line=${trace_output#*LSXPSD1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missing LSXPSD1 contract header"
    exit 0
}

diag_line="LSXPSD1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#*FX_DEFERRED_SOLID_OVERLAY_CLIP*}" = "${diag_line}" || \
    solid_apply_count=$((solid_apply_count + 1))
test "${diag_line#*FX_DEFERRED_SOLID_SKIP_ZERO_COVERAGE*}" = "${diag_line}" || \
    solid_skip_count=$((solid_skip_count + 1))
test "${diag_line#*FX_DEFERRED_SOLID_SKIP_UNSUPPRESSED*}" = "${diag_line}" || \
    solid_unsuppressed_skip_count=$((solid_unsuppressed_skip_count + 1))

test $((solid_apply_count + solid_skip_count + solid_unsuppressed_skip_count)) \
    -eq 1 || {
    echo "not ok" 1 - \
        "effects/stroke-composite must emit exactly one deferred solid replay/skip decision code"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite records deferred solid replay/skip decision"
exit 0
