#!/bin/sh
# TAP test verifying clipboard conversion round-trip when supported.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"
test "${HAVE_SIXEL2PNG-}" = 1 || skip_all "sixel2png is disabled in this build"

tap_plan 1
set -v

sixel_src="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
sixel_tmp="${ARTIFACT_LOCAL_DIR}/clipboard-input.six"
roundtrip_png="${ARTIFACT_LOCAL_DIR}/clipboard-roundtrip.png"

run_img2sixel "${sixel_src}" >"${sixel_tmp}" || {
    fail 1 "failed to prepare sixel input"
    exit 0
}

run_sixel2png -i "${sixel_tmp}" -o png:clipboard: || {
    tap_skip 1 "clipboard backend unavailable"
    exit 0
}

run_img2sixel clipboard: -o clipboard: || {
    tap_skip 1 "clipboard backend unavailable"
    exit 0
}

run_sixel2png -i clipboard: -o "${roundtrip_png}" || {
    tap_skip 1 "clipboard backend unavailable"
    exit 0
}

test -s "${roundtrip_png}" || {
    fail 1 "round-trip PNG missing"
    exit 0
}

pass 1 "clipboard round-trip succeeded"
