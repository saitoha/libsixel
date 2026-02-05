#!/bin/sh
# TAP test verifying clipboard conversion round-trip when supported.

# Enable strict mode with verbose tracing for diagnostics.
set -eux



script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

status=0

require_file "${images_dir}/snake.png"

tap_plan 1
set -v

sixel_src="${images_dir}/snake.png"
sixel_tmp="${ARTIFACT_LOCAL_DIR}/clipboard-input.six"
roundtrip_png="${ARTIFACT_LOCAL_DIR}/clipboard-roundtrip.png"

if run_img2sixel "${sixel_src}" >"${sixel_tmp}"; then
    :
else
    fail "failed to prepare sixel input"
fi

if run_sixel2png -i "${sixel_tmp}" -o png:clipboard: \
; then
    :
else
    tap_skip 1 "clipboard backend unavailable"
    exit 0
fi

if run_img2sixel clipboard: -o clipboard:; then
    :
else
    tap_skip 1 "clipboard backend unavailable"
    exit 0
fi

if run_sixel2png -i clipboard: -o "${roundtrip_png}" \
; then
    :
else
    tap_skip 1 "clipboard backend unavailable"
    exit 0
fi

if [ -s "${roundtrip_png}" ]; then
    pass "clipboard round-trip succeeded"
else
    fail "round-trip PNG missing"
fi
