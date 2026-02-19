#!/bin/sh
# TAP test: ACT palette export writes expected size.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
act_palette="${ARTIFACT_LOCAL_DIR}/palette.act"

run_img2sixel -M "${act_palette}" -o "${ARTIFACT_LOCAL_DIR}/act.six" "${snake_png}" || {
    fail 1 "ACT palette export failed"
    exit 0
}

act_size=$(wc -c <"${act_palette}")
test "${act_size}" -eq 772 || {
    fail 1 "ACT palette length mismatch (${act_size})"
    exit 0
}

pass 1 "ACT palette exported with correct length"

exit 0
