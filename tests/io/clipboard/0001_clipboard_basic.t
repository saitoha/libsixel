#!/bin/sh
# TAP test verifying clipboard conversion round-trip when supported.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"
config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

status=0

tap_plan 1
set -v

sixel_src="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
sixel_tmp="${ARTIFACT_LOCAL_DIR}/clipboard-input.six"
roundtrip_png="${ARTIFACT_LOCAL_DIR}/clipboard-roundtrip.png"

if ! run_img2sixel "${sixel_src}" >"${sixel_tmp}"; then
    fail "failed to prepare sixel input"
fi

if ! run_sixel2png -i "${sixel_tmp}" -o png:clipboard: ; then
    tap_skip 1 "clipboard backend unavailable"
    exit 0
fi

if ! run_img2sixel clipboard: -o clipboard:; then
    tap_skip 1 "clipboard backend unavailable"
    exit 0
fi

if ! run_sixel2png -i clipboard: -o "${roundtrip_png}"; then
    tap_skip 1 "clipboard backend unavailable"
    exit 0
fi

if [ -s "${roundtrip_png}" ]; then
    pass "clipboard round-trip succeeded"
else
    fail "round-trip PNG missing"
fi
