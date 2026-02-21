#!/bin/sh
# TAP test: wic animation start frame accepts negative indexes.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}
test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic support is disabled in this build\n";
    exit 0
}
test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    printf "1..0 # SKIP WIC is unavailable under wine\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel --start-frame=-1 \
    -L wic! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/wic_start_negative.six" || {
    fail 1 "wic decode with negative start frame failed"
    exit 0
}

run_img2sixel --start-frame=4 \
    -L wic! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/wic_start_positive_equivalent.six" || {
    fail 1 "wic decode with equivalent positive start frame failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/wic_start_negative.six" \
    "${ARTIFACT_LOCAL_DIR}/wic_start_positive_equivalent.six" || {
    fail 1 "negative start frame did not map to last wic frame"
    exit 0
}

pass 1 "wic negative start frame resolves from tail"
exit 0
