#!/bin/sh
# TAP test: wic animation start frame accepts positive indexes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic support is disabled in this build\n";
    exit 0
}
echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"
test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    printf "1..0 # SKIP WIC is unavailable under wine\n"
    exit 0
}


${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L wic! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/wic_start_default.six" || {
    echo "not ok" 1 - "baseline wic animation decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=1 \
    -L wic! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/wic_start_positive.six" || {
    echo "not ok" 1 - "wic decode with positive start frame failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/wic_start_default.six" \
    "${ARTIFACT_LOCAL_DIR}/wic_start_positive.six" && {
    echo "not ok" 1 - "positive start frame did not change wic output"
    exit 0
}

echo "ok" 1 - "wic positive start frame is applied"
exit 0
