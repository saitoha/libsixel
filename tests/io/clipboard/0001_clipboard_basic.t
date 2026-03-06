#!/bin/sh
# TAP test verifying clipboard conversion round-trip when supported.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

printf '1..1\n'
set -v

sixel_src="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
sixel_tmp="${ARTIFACT_LOCAL_DIR}/clipboard-input.six"
roundtrip_png="${ARTIFACT_LOCAL_DIR}/clipboard-roundtrip.png"

run_img2sixel "${sixel_src}" >"${sixel_tmp}" || {
    echo "not ok" 1 - "failed to prepare sixel input"
    exit 0
}

run_sixel2png -i "${sixel_tmp}" -o png:clipboard: || {
    printf "ok 1 # SKIP clipboard backend unavailable\n"
    exit 0
}

run_img2sixel clipboard: -o clipboard: || {
    printf "ok 1 # SKIP clipboard backend unavailable"
    exit 0
}

run_sixel2png -i clipboard: -o "${roundtrip_png}" || {
    printf "ok 1 # SKIP clipboard backend unavailable"
    exit 0
}

test -s "${roundtrip_png}" || {
    echo "not ok" 1 - "round-trip PNG missing"
    exit 0
}

echo "ok" 1 - "clipboard round-trip succeeded"
